#ifndef SYSCAPE_DETAIL_CPU_NUCLEUS_HPP
#define SYSCAPE_DETAIL_CPU_NUCLEUS_HPP

/// @file
/// @brief Nucleus RTOS backend implementation for CPU queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses <nucleus.h> when available.

#include <syscape/detail/config.hpp>

#include <cstdint>
#include <string>
#include <vector>

#if !defined(SYSCAPE_NUCLEUS_HAS_NUCLEUS_H)
#if defined(__has_include)
#if __has_include(<nucleus.h>)
#include <nucleus.h>
#define SYSCAPE_NUCLEUS_HAS_NUCLEUS_H 1
#elif __has_include("nucleus.h")
#include "nucleus.h"
#define SYSCAPE_NUCLEUS_HAS_NUCLEUS_H 1
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
#if defined(SYSCAPE_NUCLEUS_CPU_COUNT)
    if (SYSCAPE_NUCLEUS_CPU_COUNT > 0) {
        return static_cast<std::uint32_t>(SYSCAPE_NUCLEUS_CPU_COUNT);
    }
    return fail(errc::not_supported);
#elif defined(NUCLEUS_CPU_COUNT)
    if (NUCLEUS_CPU_COUNT > 0) {
        return static_cast<std::uint32_t>(NUCLEUS_CPU_COUNT);
    }
    return fail(errc::not_supported);
#elif defined(SYSCAPE_NUCLEUS_HAS_NUCLEUS_H)
#if defined(NU_NUM_CORES)
    if (NU_NUM_CORES > 0) {
        return static_cast<std::uint32_t>(NU_NUM_CORES);
    }
#endif
    const std::uint32_t cores = NU_Core_Count();
    if (cores > 0U) {
        return cores;
    }
    return 1U;
#else
    return 1U;
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

#endif // SYSCAPE_DETAIL_CPU_NUCLEUS_HPP
