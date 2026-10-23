#ifndef SYSCAPE_DETAIL_CPU_PIKEOS_HPP
#define SYSCAPE_DETAIL_CPU_PIKEOS_HPP

/// @file
/// @brief PikeOS RTOS backend implementation for CPU queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses p4_cpu_count() or SYSCAPE_PIKEOS_CPU_COUNT / PIKEOS_CPU_COUNT.

#include <syscape/detail/config.hpp>

#include <cstdint>
#include <string>
#include <vector>

#if !defined(SYSCAPE_PIKEOS_HAS_P4_H)
#if defined(__has_include)
#if __has_include(<p4.h>)
#include <p4.h>
#define SYSCAPE_PIKEOS_HAS_P4_H 1
#elif __has_include("p4.h")
#include "p4.h"
#define SYSCAPE_PIKEOS_HAS_P4_H 1
#endif
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
#elif defined(SYSCAPE_PIKEOS_HAS_P4_H)
    const P4_cpu_t cpus = p4_cpu_count();
    if (cpus > 0) {
        return static_cast<std::uint32_t>(cpus);
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

#endif // SYSCAPE_DETAIL_CPU_PIKEOS_HPP
