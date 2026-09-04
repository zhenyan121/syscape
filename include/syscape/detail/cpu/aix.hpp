#ifndef SYSCAPE_DETAIL_CPU_AIX_HPP
#define SYSCAPE_DETAIL_CPU_AIX_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#if defined(__has_include)
#if __has_include(<libperfstat.h>)
#include <libperfstat.h>
#define SYSCAPE_HAS_AIX_LIBPERFSTAT 1
#endif
#if __has_include(<sys/systemcfg.h>)
#include <sys/systemcfg.h>
#define SYSCAPE_HAS_AIX_SYSTEMCFG 1
#endif
#endif

#include <syscape/detail/cpu/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace cpu_backend {

inline result<std::vector<std::string>> vendor_identifiers() {
    return std::vector<std::string> {"IBM"};
}

inline result<std::vector<std::string>> model_names() {
    return fail(errc::not_found);
}

inline result<std::uint32_t> online_logical_processor_count() {
    const long count = ::sysconf(_SC_NPROCESSORS_ONLN);
    if (count <= 0) {
        return fail(errc::malformed_data);
    }
    if (static_cast<unsigned long>(count) >
        (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(count);
}

inline result<std::uint32_t> online_physical_core_count() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> online_processor_package_count() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> minimum_frequency_khz() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> maximum_frequency_khz() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::uint32_t>> current_frequencies_khz() {
    return fail(errc::not_supported);
}

inline result<cpu_common::cache_entry>
make_cache_entry(std::uint8_t level, cpu::cache_kind kind, std::uint32_t size,
                 std::uint32_t line_size) {
    cpu_common::cache_entry entry {};
    entry.level = level;
    entry.kind = kind;
    entry.instance_size_bytes = size;
    entry.line_size_bytes = line_size;
    return entry;
}

inline result<std::vector<cpu_common::cache_entry>> cache_descriptors() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> instruction_set_features() {
    return fail(errc::not_supported);
}

inline result<cpu_common::usage_information> cumulative_processor_usage() {
#if defined(SYSCAPE_HAS_AIX_LIBPERFSTAT)
    perfstat_cpu_total_t total {};
    if (::perfstat_cpu_total(nullptr, &total, sizeof(perfstat_cpu_total_t), 1) >
        0) {
        cpu_common::usage_information usage {};
        usage.user_ticks = static_cast<std::uint64_t>(total.user);
        usage.system_ticks = static_cast<std::uint64_t>(total.sys);
        usage.idle_ticks = static_cast<std::uint64_t>(total.idle + total.wait);
        return usage;
    }
#endif
    return fail(errc::not_supported);
}

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#endif
