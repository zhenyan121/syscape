#ifndef SYSCAPE_DETAIL_CPU_MBED_HPP
#define SYSCAPE_DETAIL_CPU_MBED_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>
#include <string>
#include <vector>

#if defined(__has_include)
#if __has_include(<mbed.h>)
#include <mbed.h>
#define SYSCAPE_MBED_HAS_KERNEL_HEADERS 1
#elif __has_include(<cmsis_os2.h>)
#include <cmsis_os2.h>
#define SYSCAPE_MBED_HAS_KERNEL_HEADERS 1
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
#if defined(MBED_CPU_COUNT) && (MBED_CPU_COUNT > 0)
    return static_cast<std::uint32_t>(MBED_CPU_COUNT);
#elif defined(MBED_CONF_RTOS_CPU_COUNT) && (MBED_CONF_RTOS_CPU_COUNT > 0)
    return static_cast<std::uint32_t>(MBED_CONF_RTOS_CPU_COUNT);
#elif defined(NUM_CORES) && (NUM_CORES > 0)
    return static_cast<std::uint32_t>(NUM_CORES);
#elif defined(SYSCAPE_MBED_HAS_KERNEL_HEADERS)
    return 1U;
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

#if defined(SYSCAPE_MBED_HAS_KERNEL_HEADERS)
#undef SYSCAPE_MBED_HAS_KERNEL_HEADERS
#endif

#endif
