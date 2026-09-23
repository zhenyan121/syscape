#ifndef SYSCAPE_DETAIL_CPU_OS2_HPP
#define SYSCAPE_DETAIL_CPU_OS2_HPP

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
#if defined(SYSCAPE_OS2_CPU_COUNT)
    return static_cast<std::uint32_t>(SYSCAPE_OS2_CPU_COUNT);
#else
    return static_cast<std::uint32_t>(1U);
#endif
}

inline result<std::uint32_t> online_physical_core_count() {
    return online_logical_processor_count();
}

inline result<std::uint32_t> online_processor_package_count() {
    return static_cast<std::uint32_t>(1U);
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

#endif
