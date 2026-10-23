#ifndef SYSCAPE_DETAIL_CPU_ECOS_HPP
#define SYSCAPE_DETAIL_CPU_ECOS_HPP

/// @file
/// @brief eCos RTOS backend implementation for CPU queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses CYGNUM_KERNEL_CPU_MAX / CYG_KERNEL_SMP_CPUS /
/// SYSCAPE_ECOS_CPU_COUNT.

#include <syscape/detail/config.hpp>

#include <cstdint>
#include <string>
#include <vector>

#if !defined(SYSCAPE_ECOS_HAS_KAPI_H)
#if defined(__has_include)
#if __has_include(<cyg/kernel/kapi.h>)
#include <cyg/kernel/kapi.h>
#define SYSCAPE_ECOS_HAS_KAPI_H 1
#elif __has_include("cyg/kernel/kapi.h")
#include "cyg/kernel/kapi.h"
#define SYSCAPE_ECOS_HAS_KAPI_H 1
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
#if defined(SYSCAPE_ECOS_CPU_COUNT)
    static_assert(SYSCAPE_ECOS_CPU_COUNT > 0,
                  "SYSCAPE_ECOS_CPU_COUNT must be positive");
    return static_cast<std::uint32_t>(SYSCAPE_ECOS_CPU_COUNT);
#elif defined(ECOS_CPU_COUNT)
    static_assert(ECOS_CPU_COUNT > 0, "ECOS_CPU_COUNT must be positive");
    return static_cast<std::uint32_t>(ECOS_CPU_COUNT);
#elif defined(HAL_SMP_CPU_COUNT)
    const auto count = HAL_SMP_CPU_COUNT;
    if (count > 0) {
        return static_cast<std::uint32_t>(count);
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

#endif // SYSCAPE_DETAIL_CPU_ECOS_HPP
