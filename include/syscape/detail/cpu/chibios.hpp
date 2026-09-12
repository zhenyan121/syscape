#ifndef SYSCAPE_DETAIL_CPU_CHIBIOS_HPP
#define SYSCAPE_DETAIL_CPU_CHIBIOS_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>
#include <string>
#include <vector>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<ch.h>)
#include <ch.h>
#define SYSCAPE_CHIBIOS_HAS_KERNEL_HEADERS 1
#endif
#endif
#if defined(__cplusplus)
}
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
#if defined(PORT_CORES_NUMBER)
    return static_cast<std::uint32_t>(PORT_CORES_NUMBER);
#elif defined(CH_PORT_CORES_NUMBER)
    return static_cast<std::uint32_t>(CH_PORT_CORES_NUMBER);
#elif defined(CH_NUM_CORES)
    return static_cast<std::uint32_t>(CH_NUM_CORES);
#elif defined(SYSCAPE_CHIBIOS_HAS_KERNEL_HEADERS)
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

#if defined(SYSCAPE_CHIBIOS_HAS_KERNEL_HEADERS)
#undef SYSCAPE_CHIBIOS_HAS_KERNEL_HEADERS
#endif

#endif
