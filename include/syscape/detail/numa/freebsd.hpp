#ifndef SYSCAPE_DETAIL_NUMA_FREEBSD_HPP
#define SYSCAPE_DETAIL_NUMA_FREEBSD_HPP

#include <cerrno>
#include <cstdint>
#include <limits>
#include <optional>
#include <sched.h>
#include <sys/cpuset.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/numa.hpp>
#include <syscape/detail/numa/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace numa_backend {

inline result<int> read_sysctl_int(const char* name) {
    int value = 0;
    std::size_t size = sizeof(value);
    if (::sysctlbyname(name, &value, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    return value;
}

inline result<::syscape::numa::numa_node> read_node(std::uint32_t node_id) {
    const result<int> ndomains_res = read_sysctl_int("vm.ndomains");
    const int ndomains =
        (ndomains_res && *ndomains_res > 0) ? *ndomains_res : 1;
    if (node_id >= static_cast<std::uint32_t>(ndomains)) {
        return fail(errc::not_found);
    }

    ::syscape::numa::numa_node node;
    node.id = node_id;
    node.is_online = true;

    if (ndomains == 1) {
        // Single UMA domain: all CPUs and total memory belong to node 0
        int ncpu = 0;
        std::size_t size = sizeof(ncpu);
        if (::sysctlbyname("hw.ncpu", &ncpu, &size, nullptr, 0U) == 0 &&
            ncpu > 0) {
            node.logical_processors.reserve(static_cast<std::size_t>(ncpu));
            for (int i = 0; i < ncpu; ++i) {
                node.logical_processors.push_back(
                    static_cast<std::uint32_t>(i));
            }
        }

        std::uint64_t physmem = 0U;
        size = sizeof(physmem);
        if (::sysctlbyname("hw.physmem", &physmem, &size, nullptr, 0U) == 0 &&
            physmem > 0U) {
            node.total_memory_bytes = physmem;
        }
    } else {
        // Multi-domain NUMA: query domain CPU set via cpuset_getaffinity
        cpuset_t domain_mask;
        CPU_ZERO(&domain_mask);
        if (::cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_DOMAIN,
                                 static_cast<id_t>(node_id),
                                 sizeof(domain_mask), &domain_mask) != 0) {
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            if (errno != ESRCH && errno != ENOENT) {
                return fail(std::error_code(errno, std::generic_category()));
            }
        } else {
            int ncpu = 0;
            std::size_t size = sizeof(ncpu);
            if (::sysctlbyname("hw.ncpu", &ncpu, &size, nullptr, 0U) != 0 ||
                ncpu <= 0) {
                ncpu = CPU_SETSIZE;
            }
            for (int i = 0; i < ncpu && i < CPU_SETSIZE; ++i) {
                if (CPU_ISSET(i, &domain_mask)) {
                    node.logical_processors.push_back(
                        static_cast<std::uint32_t>(i));
                }
            }
        }
        // Per-domain memory is not exposed via global hw.physmem; left
        // disengaged
    }

    return numa_common::validate_numa_node(std::move(node));
}

inline result<bool> is_numa_available() {
    const result<int> ndomains = read_sysctl_int("vm.ndomains");
    if (ndomains) {
        return *ndomains > 1;
    }
    return false;
}

inline result<std::uint32_t> node_count() {
    const result<int> ndomains = read_sysctl_int("vm.ndomains");
    if (ndomains && *ndomains > 0) {
        return static_cast<std::uint32_t>(*ndomains);
    }
    return 1U;
}

inline result<std::vector<::syscape::numa::numa_node>> nodes() {
    const result<std::uint32_t> count = node_count();
    if (!count) {
        return fail(count.error());
    }
    std::vector<::syscape::numa::numa_node> list;
    list.reserve(*count);
    for (std::uint32_t i = 0U; i < *count; ++i) {
        auto n = read_node(i);
        if (!n) {
            return fail(n.error());
        }
        list.push_back(std::move(*n));
    }
    return list;
}

inline result<::syscape::numa::numa_node> node(std::uint32_t id) {
    return read_node(id);
}

inline result<std::uint32_t> current_thread_node() {
    const result<int> ndomains = read_sysctl_int("vm.ndomains");
    if (!ndomains) {
        if (ndomains.error() == errc::not_found) {
            return 0U;
        }
        return fail(ndomains.error());
    }
    if (*ndomains <= 1) {
        return 0U;
    }

    const int current_cpu = ::sched_getcpu();
    if (current_cpu < 0) {
        return fail(errc::not_supported);
    }

    for (int d = 0; d < *ndomains; ++d) {
        cpuset_t domain_mask;
        CPU_ZERO(&domain_mask);
        if (::cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_DOMAIN,
                                 static_cast<id_t>(d), sizeof(domain_mask),
                                 &domain_mask) != 0) {
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            if (errno == ESRCH || errno == ENOENT) {
                continue;
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (current_cpu < CPU_SETSIZE && CPU_ISSET(current_cpu, &domain_mask)) {
            return static_cast<std::uint32_t>(d);
        }
    }

    return fail(errc::not_supported);
}

} // namespace numa_backend
} // namespace detail
} // namespace syscape

#endif
