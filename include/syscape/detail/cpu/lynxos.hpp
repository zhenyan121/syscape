#ifndef SYSCAPE_DETAIL_CPU_LYNXOS_HPP
#define SYSCAPE_DETAIL_CPU_LYNXOS_HPP

/// @file
/// @brief LynxOS / LynxOS-178 RTOS backend implementation for CPU queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses sysconf(_SC_NPROCESSORS_ONLN) or SYSCAPE_LYNXOS_CPU_COUNT.

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

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
#if defined(SYSCAPE_LYNXOS_CPU_COUNT)
    static_assert(SYSCAPE_LYNXOS_CPU_COUNT > 0,
                  "SYSCAPE_LYNXOS_CPU_COUNT must be positive");
    return static_cast<std::uint32_t>(SYSCAPE_LYNXOS_CPU_COUNT);
#elif defined(LYNXOS_CPU_COUNT)
    static_assert(LYNXOS_CPU_COUNT > 0, "LYNXOS_CPU_COUNT must be positive");
    return static_cast<std::uint32_t>(LYNXOS_CPU_COUNT);
#elif defined(_SC_NPROCESSORS_ONLN)
    errno = 0;
    const long count = ::sysconf(_SC_NPROCESSORS_ONLN);
    if (count > 0) {
        if (static_cast<unsigned long>(count) >
            (std::numeric_limits<std::uint32_t>::max)()) {
            return fail(errc::value_too_large);
        }
        return static_cast<std::uint32_t>(count);
    }
    const int err = errno;
    if (count == 0) {
        return fail(errc::malformed_data);
    }
    if (err != 0 && err != EINVAL) {
        return fail(std::error_code(err, std::generic_category()));
    }
    return fail(errc::not_supported);
#else
    return fail(errc::not_supported);
#endif
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

#endif // SYSCAPE_DETAIL_CPU_LYNXOS_HPP
