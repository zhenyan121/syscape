#ifndef SYSCAPE_DETAIL_CPU_PIKEOS_HPP
#define SYSCAPE_DETAIL_CPU_PIKEOS_HPP

/// @file
/// @brief PikeOS RTOS backend implementation for CPU queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses SYSCAPE_PIKEOS_CPU_COUNT / PIKEOS_CPU_COUNT / P4_CPU_COUNT.

#include <syscape/detail/config.hpp>

#include <cstdint>
#include <string>
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
#if defined(SYSCAPE_PIKEOS_CPU_COUNT)
    static_assert(SYSCAPE_PIKEOS_CPU_COUNT > 0,
                  "SYSCAPE_PIKEOS_CPU_COUNT must be positive");
    return static_cast<std::uint32_t>(SYSCAPE_PIKEOS_CPU_COUNT);
#elif defined(PIKEOS_CPU_COUNT)
    static_assert(PIKEOS_CPU_COUNT > 0, "PIKEOS_CPU_COUNT must be positive");
    return static_cast<std::uint32_t>(PIKEOS_CPU_COUNT);
#elif defined(P4_CPU_COUNT)
    static_assert(P4_CPU_COUNT > 0, "P4_CPU_COUNT must be positive");
    return static_cast<std::uint32_t>(P4_CPU_COUNT);
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

#endif // SYSCAPE_DETAIL_CPU_PIKEOS_HPP
