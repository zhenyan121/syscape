#ifndef SYSCAPE_DETAIL_CPU_HPUX_HPP
#define SYSCAPE_DETAIL_CPU_HPUX_HPP

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#if defined(__has_include)
#if __has_include(<sys/pstat.h>)
#include <sys/pstat.h>
#define SYSCAPE_HAS_HPUX_PSTAT 1
#endif
#endif

#include <syscape/detail/cpu/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace cpu_backend {

inline result<std::vector<std::string>> vendor_identifiers() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> model_names() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> online_logical_processor_count() {
    errno = 0;
    const long count = ::sysconf(_SC_NPROCESSORS_ONLN);
    if (count < 0) {
        const int saved_errno = errno;
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    if (count == 0) {
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
#if defined(SYSCAPE_HAS_HPUX_PSTAT)
    errno = 0;
    const long configured_count = ::sysconf(_SC_NPROCESSORS_CONF);
    if (configured_count < 0) {
        const int saved_errno = errno;
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    if (configured_count == 0) {
        return fail(errc::malformed_data);
    }
    if (static_cast<unsigned long>(configured_count) >
        static_cast<unsigned long>((std::numeric_limits<int>::max)())) {
        return fail(errc::value_too_large);
    }
    std::vector<struct pst_processor> processors(
        static_cast<std::size_t>(configured_count));
    errno = 0;
    const int processor_count = ::pstat_getprocessor(
        processors.data(), sizeof(struct pst_processor), processors.size(), 0);
    if (processor_count < 0) {
        const int saved_errno = errno;
        if (saved_errno == EACCES || saved_errno == EPERM) {
            return fail(errc::permission_denied);
        }
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::io_error);
    }
    if (processor_count == 0) {
        return fail(errc::temporarily_unavailable);
    }
    if (static_cast<std::size_t>(processor_count) > processors.size()) {
        return fail(errc::malformed_data);
    }
    cpu_common::usage_information total_usage {};
    std::size_t enabled_count = 0U;
    for (int i = 0; i < processor_count; ++i) {
        const struct pst_processor& psp =
            processors[static_cast<std::size_t>(i)];
        if (psp.psp_processor_state != PSP_SPU_ENABLED) {
            continue;
        }
        ++enabled_count;
#if defined(CP_USER) && defined(CP_SYS) && defined(CP_IDLE)
        std::uint64_t user =
            static_cast<std::uint64_t>(psp.psp_cpu_time[CP_USER]);
#if defined(CP_NICE)
        const auto nice_ticks =
            static_cast<std::uint64_t>(psp.psp_cpu_time[CP_NICE]);
        if (UINT64_MAX - user < nice_ticks) {
            return fail(errc::value_too_large);
        }
        user += nice_ticks;
#endif

        std::uint64_t sys =
            static_cast<std::uint64_t>(psp.psp_cpu_time[CP_SYS]);
#if defined(CP_SSYS)
        const auto ssys_ticks =
            static_cast<std::uint64_t>(psp.psp_cpu_time[CP_SSYS]);
        if (UINT64_MAX - sys < ssys_ticks) {
            return fail(errc::value_too_large);
        }
        sys += ssys_ticks;
#endif

        std::uint64_t idle =
            static_cast<std::uint64_t>(psp.psp_cpu_time[CP_IDLE]);
#if defined(CP_WAIT)
        const auto wait_ticks =
            static_cast<std::uint64_t>(psp.psp_cpu_time[CP_WAIT]);
        if (UINT64_MAX - idle < wait_ticks) {
            return fail(errc::value_too_large);
        }
        idle += wait_ticks;
#endif

        if (UINT64_MAX - total_usage.user_ticks < user ||
            UINT64_MAX - total_usage.system_ticks < sys ||
            UINT64_MAX - total_usage.idle_ticks < idle) {
            return fail(errc::value_too_large);
        }
        total_usage.user_ticks += user;
        total_usage.system_ticks += sys;
        total_usage.idle_ticks += idle;
#else
        return fail(errc::not_supported);
#endif
    }
    if (enabled_count == 0U) {
        return fail(errc::temporarily_unavailable);
    }
    return total_usage;
#else
    return fail(errc::not_supported);
#endif
}

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#endif
