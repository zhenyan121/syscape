#ifndef SYSCAPE_DETAIL_CPU_TI_RTOS_HPP
#define SYSCAPE_DETAIL_CPU_TI_RTOS_HPP

/// @file
/// @brief TI-RTOS (SYS/BIOS) backend implementation for CPU queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Discovers logical processor count via ti_sysbios_hal_Core_numCores,
/// Core_numCores,
///       SYSCAPE_TI_RTOS_CPU_COUNT, or TI_RTOS_CPU_COUNT; reports not_supported
///       otherwise.

#include <syscape/detail/config.hpp>

#include <cstdint>
#include <string>
#include <vector>

#if defined(__has_include)
#if __has_include(<ti/sysbios/hal/Core.h>)
#include <ti/sysbios/hal/Core.h>
#define SYSCAPE_TI_RTOS_HAS_CORE_H 1
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
#if defined(SYSCAPE_TI_RTOS_CPU_COUNT) && (SYSCAPE_TI_RTOS_CPU_COUNT > 0)
    return static_cast<std::uint32_t>(SYSCAPE_TI_RTOS_CPU_COUNT);
#elif defined(TI_RTOS_CPU_COUNT) && (TI_RTOS_CPU_COUNT > 0)
    return static_cast<std::uint32_t>(TI_RTOS_CPU_COUNT);
#elif defined(SYSCAPE_TI_RTOS_HAS_CORE_H)
    const auto cores = static_cast<std::uint32_t>(ti_sysbios_hal_Core_numCores);
    if (cores > 0U) {
        return cores;
    }
    return fail(errc::not_supported);
#elif defined(Core_numCores)
    const auto cores = static_cast<std::uint32_t>(Core_numCores);
    if (cores > 0U) {
        return cores;
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

#if defined(SYSCAPE_TI_RTOS_HAS_CORE_H)
#undef SYSCAPE_TI_RTOS_HAS_CORE_H
#endif

#endif // SYSCAPE_DETAIL_CPU_TI_RTOS_HPP
