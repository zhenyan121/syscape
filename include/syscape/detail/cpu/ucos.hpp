#ifndef SYSCAPE_DETAIL_CPU_UCOS_HPP
#define SYSCAPE_DETAIL_CPU_UCOS_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>
#include <string>
#include <vector>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<os.h>)
#include <os.h>
#define SYSCAPE_UCOS_HAS_KERNEL_HEADERS 1
#elif __has_include(<ucos_ii.h>)
#include <ucos_ii.h>
#define SYSCAPE_UCOS_HAS_KERNEL_HEADERS 1
#elif __has_include(<ucos_iii.h>)
#include <ucos_iii.h>
#define SYSCAPE_UCOS_HAS_KERNEL_HEADERS 1
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
#if defined(OS_CORE_NUM_CORES)
    return static_cast<std::uint32_t>(OS_CORE_NUM_CORES);
#elif defined(OS_SMP_MAX_CORES)
    return static_cast<std::uint32_t>(OS_SMP_MAX_CORES);
#elif defined(OS_CFG_SMP_EN) && (OS_CFG_SMP_EN > 0) && defined(LIB_DEF_MAX_CPU)
    return static_cast<std::uint32_t>(LIB_DEF_MAX_CPU);
#else
    return static_cast<std::uint32_t>(1U);
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

#if defined(SYSCAPE_UCOS_HAS_KERNEL_HEADERS)
#undef SYSCAPE_UCOS_HAS_KERNEL_HEADERS
#endif

#endif
