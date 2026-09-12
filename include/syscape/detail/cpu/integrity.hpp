#ifndef SYSCAPE_DETAIL_CPU_INTEGRITY_HPP
#define SYSCAPE_DETAIL_CPU_INTEGRITY_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>
#include <string>
#include <vector>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<INTEGRITY.h>)
#include <INTEGRITY.h>
#define SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS 1
#elif __has_include(<integrity.h>)
#include <integrity.h>
#define SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS 1
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
#if defined(SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS)
    ProcessorCount count = 0;
    if (::GetProcessorCount(&count) == 0 && count > 0) {
        return static_cast<std::uint32_t>(count);
    }
    return fail(errc::temporarily_unavailable);
#elif defined(INTEGRITY_NUM_PROCESSORS)
    return static_cast<std::uint32_t>(INTEGRITY_NUM_PROCESSORS);
#elif defined(INTEGRITY_MAX_PROCESSORS)
    return static_cast<std::uint32_t>(INTEGRITY_MAX_PROCESSORS);
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

#if defined(SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS)
#undef SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS
#endif

#endif
