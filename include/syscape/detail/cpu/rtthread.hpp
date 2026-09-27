#ifndef SYSCAPE_DETAIL_CPU_RTTHREAD_HPP
#define SYSCAPE_DETAIL_CPU_RTTHREAD_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>
#include <string>
#include <vector>

#if !defined(SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS)
#define SYSCAPE_RTTHREAD_INTERNAL_KERNEL_HEADERS 1
#endif

#if defined(__has_include)
#if __has_include(<rtthread.h>)
#include <rtthread.h>
#define SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS 1
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
#if defined(RT_CPUS_NR) && (RT_CPUS_NR > 0)
    return static_cast<std::uint32_t>(RT_CPUS_NR);
#elif defined(SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS)
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

#if defined(SYSCAPE_RTTHREAD_INTERNAL_KERNEL_HEADERS)
#undef SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS
#undef SYSCAPE_RTTHREAD_INTERNAL_KERNEL_HEADERS
#endif

#endif
