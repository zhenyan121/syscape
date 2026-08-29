#ifndef SYSCAPE_DETAIL_NUMA_MACOS_HPP
#define SYSCAPE_DETAIL_NUMA_MACOS_HPP

#include <cerrno>
#include <cstdint>
#include <limits>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <mach/host_info.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <sys/sysctl.h>

#include <syscape/detail/numa/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace numa_backend {

/// Owns one Mach host send right for the duration of a single query.
class host_port {
public:
    host_port() noexcept : value_(::mach_host_self()) {}
    host_port(const host_port&) = delete;
    host_port& operator=(const host_port&) = delete;
    ~host_port() {
        if (value_ != MACH_PORT_NULL) {
            ::mach_port_deallocate(::mach_task_self(), value_);
        }
    }

    /// Returns the owned port, or MACH_PORT_NULL after a failed acquisition.
    ::mach_port_t get() const noexcept { return value_; }

private:
    ::mach_port_t value_;
};

inline result<::syscape::numa::numa_node> read_single_node(std::uint32_t node_id) {
    if (node_id != 0U) {
        return fail(errc::not_found);
    }

    ::syscape::numa::numa_node node;
    node.id = 0U;
    node.is_online = true;

    // Query logical CPUs
    int logical_cpus = 0;
    std::size_t size = sizeof(logical_cpus);
    if (::sysctlbyname("hw.logicalcpu", &logical_cpus, &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (logical_cpus <= 0) {
        return fail(errc::malformed_data);
    }
    node.logical_processors.reserve(static_cast<std::size_t>(logical_cpus));
    for (int i = 0; i < logical_cpus; ++i) {
        node.logical_processors.push_back(static_cast<std::uint32_t>(i));
    }

    // Query total memory
    std::uint64_t memsize = 0U;
    size = sizeof(memsize);
    if (::sysctlbyname("hw.memsize", &memsize, &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    node.total_memory_bytes = memsize;

    // Query free memory via Mach host statistics
    const host_port host;
    if (host.get() == MACH_PORT_NULL) {
        return fail(errc::io_error);
    }
    ::vm_size_t page_size = 0U;
    if (::host_page_size(host.get(), &page_size) != KERN_SUCCESS) {
        return fail(errc::io_error);
    }
    if (page_size == 0U) {
        return fail(errc::malformed_data);
    }
    ::vm_statistics64_data_t vm_stat{};
    ::mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (::host_statistics64(host.get(), HOST_VM_INFO64,
                            reinterpret_cast<host_info64_t>(&vm_stat),
                            &count) != KERN_SUCCESS) {
        return fail(errc::io_error);
    }
    const auto u_page_size = static_cast<std::uint64_t>(page_size);
    const auto u_free_count = static_cast<std::uint64_t>(vm_stat.free_count);
    if (u_free_count > std::numeric_limits<std::uint64_t>::max() / u_page_size) {
        return fail(errc::malformed_data);
    }
    const std::uint64_t free_bytes = u_free_count * u_page_size;
    node.free_memory_bytes = free_bytes;
    if (memsize >= free_bytes) {
        node.used_memory_bytes = memsize - free_bytes;
    }

    // macOS UMA does not expose an inter-node distance matrix.
    // distances remains empty per public contract.

    return numa_common::validate_numa_node(std::move(node));
}

inline result<bool> is_numa_available() {
    return false;
}

inline result<std::uint32_t> node_count() {
    return 1U;
}

inline result<std::vector<::syscape::numa::numa_node>> nodes() {
    auto n = read_single_node(0U);
    if (!n) { return fail(n.error()); }
    return std::vector<::syscape::numa::numa_node>{std::move(*n)};
}

inline result<::syscape::numa::numa_node> node(std::uint32_t id) {
    return read_single_node(id);
}

inline result<std::uint32_t> current_thread_node() {
    return 0U;
}

} // namespace numa_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_NUMA_MACOS_HPP
