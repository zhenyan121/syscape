#ifndef SYSCAPE_DETAIL_CPU_HAIKU_HPP
#define SYSCAPE_DETAIL_CPU_HAIKU_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <unistd.h>
#include <vector>

#if defined(__has_include)
#if __has_include(<OS.h>)
#include <OS.h>
#define SYSCAPE_HAS_HAIKU_OS_H 1
#endif
#endif

#include <syscape/detail/cpu/common.hpp>
#include <syscape/detail/haiku/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace cpu_backend {

#if defined(SYSCAPE_HAS_HAIKU_OS_H)
template <typename T>
inline auto is_cpu_active(const T& info, int) -> decltype(info.enabled != 0) {
    return info.enabled != 0;
}

template <typename T>
inline auto is_cpu_active(const T& info, long) -> decltype(info.active != 0) {
    return info.active != 0;
}
#endif

inline result<std::vector<std::string>> vendor_identifiers() {
#if (defined(__x86_64__) || defined(__i386__)) &&                              \
    defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::cpuid_info cinfo {};
    if (::get_cpuid(&cinfo, 0, 0) == B_OK) {
        char vendor[13] = {};
        std::memcpy(vendor, cinfo.eax_0.vendor_id, 12);
        if (vendor[0] != '\0') {
            return std::vector<std::string> {std::string(vendor)};
        }
    }
#endif
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> model_names() {
#if (defined(__x86_64__) || defined(__i386__)) &&                              \
    defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::cpuid_info ext {};
    if (::get_cpuid(&ext, 0x80000000, 0) == B_OK &&
        ext.regs.eax >= 0x80000004) {
        char brand[49] = {};
        for (std::uint32_t i = 0; i < 3; ++i) {
            ::cpuid_info binfo {};
            if (::get_cpuid(&binfo, 0x80000002 + i, 0) == B_OK) {
                std::memcpy(brand + i * 16, binfo.as_chars, 16);
            }
        }
        std::string s(brand);
        const auto start = s.find_first_not_of(" \t\r\n");
        if (start != std::string::npos) {
            const auto end = s.find_last_not_of(" \t\r\n");
            s = s.substr(start, end - start + 1U);
            if (!s.empty()) {
                return std::vector<std::string> {s};
            }
        }
    }
#endif
    return fail(errc::not_supported);
}

inline result<std::uint32_t> online_logical_processor_count() {
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::system_info info {};
    const status_t st = ::get_system_info(&info);
    if (st == B_OK && info.cpu_count > 0) {
        const auto count = static_cast<std::uint32_t>(info.cpu_count);
        std::vector<::cpu_info> cpus(count);
        if (::get_cpu_info(0, count, cpus.data()) == B_OK) {
            std::uint32_t active_count = 0;
            for (const auto& c : cpus) {
                if (is_cpu_active(c, 0)) {
                    ++active_count;
                }
            }
            if (active_count > 0) {
                return active_count;
            }
        }
    }
#endif
    const long n = ::sysconf(_SC_NPROCESSORS_ONLN);
    if (n > 0) {
        return static_cast<std::uint32_t>(n);
    }
    return fail(errc::not_supported);
}

inline result<std::uint32_t> online_physical_core_count() {
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::system_info info {};
    const status_t s_st = ::get_system_info(&info);
    if (s_st != B_OK) {
        return fail(haiku_error::make_haiku_error(s_st));
    }
    if (info.cpu_count <= 0) {
        return fail(errc::malformed_data);
    }
    const auto count = static_cast<std::uint32_t>(info.cpu_count);
    std::vector<::cpu_info> cpus(count);
    const status_t c_st = ::get_cpu_info(0, count, cpus.data());
    if (c_st != B_OK) {
        return fail(haiku_error::make_haiku_error(c_st));
    }

    uint32 topo_count = 0;
    const status_t t_st = ::get_cpu_topology_info(nullptr, &topo_count);
    if (t_st != B_OK) {
        return fail(haiku_error::make_haiku_error(t_st));
    }
    if (topo_count == 0) {
        return fail(errc::malformed_data);
    }
    std::vector<::cpu_topology_node_info> nodes(topo_count);
    const status_t t2_st = ::get_cpu_topology_info(nodes.data(), &topo_count);
    if (t2_st != B_OK) {
        return fail(haiku_error::make_haiku_error(t2_st));
    }

    std::uint32_t online_cores = 0;
    for (uint32 i = 0; i < topo_count; ++i) {
        if (nodes[i].type == B_TOPOLOGY_CORE) {
            bool has_online_cpu = false;
            bool has_smt_child = false;
            for (uint32 j = i + 1; j < topo_count; ++j) {
                if (nodes[j].type == B_TOPOLOGY_CORE ||
                    nodes[j].type == B_TOPOLOGY_PACKAGE ||
                    nodes[j].type == B_TOPOLOGY_ROOT) {
                    break;
                }
                if (nodes[j].type == B_TOPOLOGY_SMT) {
                    has_smt_child = true;
                    const auto cpu_id = nodes[j].id;
                    if (cpu_id < count && is_cpu_active(cpus[cpu_id], 0)) {
                        has_online_cpu = true;
                    }
                }
            }
            if (!has_smt_child && nodes[i].id < count &&
                is_cpu_active(cpus[nodes[i].id], 0)) {
                has_online_cpu = true;
            }
            if (has_online_cpu) {
                ++online_cores;
            }
        }
    }
    if (online_cores > 0) {
        return online_cores;
    }
#endif
    return fail(errc::not_supported);
}

inline result<std::uint32_t> online_processor_package_count() {
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::system_info info {};
    const status_t s_st = ::get_system_info(&info);
    if (s_st != B_OK) {
        return fail(haiku_error::make_haiku_error(s_st));
    }
    if (info.cpu_count <= 0) {
        return fail(errc::malformed_data);
    }
    const auto count = static_cast<std::uint32_t>(info.cpu_count);
    std::vector<::cpu_info> cpus(count);
    const status_t c_st = ::get_cpu_info(0, count, cpus.data());
    if (c_st != B_OK) {
        return fail(haiku_error::make_haiku_error(c_st));
    }

    uint32 topo_count = 0;
    const status_t t_st = ::get_cpu_topology_info(nullptr, &topo_count);
    if (t_st != B_OK) {
        return fail(haiku_error::make_haiku_error(t_st));
    }
    if (topo_count == 0) {
        return fail(errc::malformed_data);
    }
    std::vector<::cpu_topology_node_info> nodes(topo_count);
    const status_t t2_st = ::get_cpu_topology_info(nodes.data(), &topo_count);
    if (t2_st != B_OK) {
        return fail(haiku_error::make_haiku_error(t2_st));
    }

    std::uint32_t online_packages = 0;
    for (uint32 i = 0; i < topo_count; ++i) {
        if (nodes[i].type == B_TOPOLOGY_PACKAGE) {
            bool has_online_cpu = false;
            bool has_child_cpu = false;
            for (uint32 j = i + 1; j < topo_count; ++j) {
                if (nodes[j].type == B_TOPOLOGY_PACKAGE ||
                    nodes[j].type == B_TOPOLOGY_ROOT) {
                    break;
                }
                if (nodes[j].type == B_TOPOLOGY_SMT) {
                    has_child_cpu = true;
                    const auto cpu_id = nodes[j].id;
                    if (cpu_id < count && is_cpu_active(cpus[cpu_id], 0)) {
                        has_online_cpu = true;
                    }
                } else if (nodes[j].type == B_TOPOLOGY_CORE) {
                    has_child_cpu = true;
                    const auto cpu_id = nodes[j].id;
                    if (cpu_id < count && is_cpu_active(cpus[cpu_id], 0)) {
                        has_online_cpu = true;
                    }
                }
            }
            if (!has_child_cpu && nodes[i].id < count &&
                is_cpu_active(cpus[nodes[i].id], 0)) {
                has_online_cpu = true;
            }
            if (has_online_cpu) {
                ++online_packages;
            }
        }
    }
    if (online_packages > 0) {
        return online_packages;
    }
#endif
    return fail(errc::not_supported);
}

inline result<std::uint32_t> minimum_frequency_khz() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> maximum_frequency_khz() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::uint32_t>> current_frequencies_khz() {
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::system_info info {};
    const status_t s_st = ::get_system_info(&info);
    if (s_st != B_OK) {
        return fail(haiku_error::make_haiku_error(s_st));
    }
    if (info.cpu_count <= 0) {
        return fail(errc::malformed_data);
    }
    const auto count = static_cast<std::uint32_t>(info.cpu_count);
    std::vector<::cpu_info> cpus(count);
    const status_t c_st = ::get_cpu_info(0, count, cpus.data());
    if (c_st != B_OK) {
        return fail(haiku_error::make_haiku_error(c_st));
    }
    std::vector<std::uint32_t> freqs;
    freqs.reserve(count);
    for (const auto& c : cpus) {
        if (!is_cpu_active(c, 0)) {
            continue;
        }
        if (c.current_frequency == 0) {
            return fail(errc::temporarily_unavailable);
        }
        const auto khz =
            static_cast<std::uint64_t>(c.current_frequency / 1000ULL);
        if (khz == 0 || khz > (std::numeric_limits<std::uint32_t>::max)()) {
            return fail(errc::malformed_data);
        }
        freqs.push_back(static_cast<std::uint32_t>(khz));
    }
    if (freqs.empty()) {
        return fail(errc::not_supported);
    }
    return freqs;
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::vector<cpu_common::cache_entry>> cache_descriptors() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> instruction_set_features() {
    return fail(errc::not_supported);
}

inline result<cpu_common::usage_information> cumulative_processor_usage() {
    return fail(errc::not_supported);
}

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#endif
