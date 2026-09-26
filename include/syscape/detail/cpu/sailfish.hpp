#ifndef SYSCAPE_DETAIL_CPU_SAILFISH_HPP
#define SYSCAPE_DETAIL_CPU_SAILFISH_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include <syscape/detail/cpu/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace cpu_backend {

template <typename T>
inline result<std::uint32_t> validate_positive_u32_count(T value) {
    if constexpr (std::is_signed<T>::value) {
        if (value <= 0) {
            return fail(errc::malformed_data);
        }
    } else {
        if (value == 0) {
            return fail(errc::malformed_data);
        }
    }
    if (static_cast<unsigned long long>(value) >
        static_cast<unsigned long long>(
            (std::numeric_limits<std::uint32_t>::max)())) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(value);
}

/// Vendor identifiers are not available in unprivileged sandboxes without
/// procfs.
inline result<std::vector<std::string>> vendor_identifiers() {
    return fail(errc::not_supported);
}

/// Model names are not available in unprivileged sandboxes without procfs.
inline result<std::vector<std::string>> model_names() {
    return fail(errc::not_supported);
}

/// Returns the online logical processor count if supplied by override macro.
inline result<std::uint32_t> online_logical_processor_count() {
#if defined(SYSCAPE_SAILFISH_CPU_COUNT)
    return validate_positive_u32_count(SYSCAPE_SAILFISH_CPU_COUNT);
#else
    return fail(errc::not_supported);
#endif
}

/// Returns the online physical core count if supplied by override macro.
inline result<std::uint32_t> online_physical_core_count() {
#if defined(SYSCAPE_SAILFISH_PHYSICAL_CPU_COUNT)
    return validate_positive_u32_count(SYSCAPE_SAILFISH_PHYSICAL_CPU_COUNT);
#else
    return fail(errc::not_supported);
#endif
}

/// Returns the online processor package count if supplied by override macro.
inline result<std::uint32_t> online_processor_package_count() {
#if defined(SYSCAPE_SAILFISH_PACKAGE_COUNT)
    return validate_positive_u32_count(SYSCAPE_SAILFISH_PACKAGE_COUNT);
#else
    return fail(errc::not_supported);
#endif
}

/// Minimum frequency query is not supported without privileged access.
inline result<std::uint32_t> minimum_frequency_khz() {
    return fail(errc::not_supported);
}

/// Maximum frequency query is not supported without privileged access.
inline result<std::uint32_t> maximum_frequency_khz() {
    return fail(errc::not_supported);
}

/// Current frequencies query is not supported without privileged access.
inline result<std::vector<std::uint32_t>> current_frequencies_khz() {
    return fail(errc::not_supported);
}

/// Cache descriptors are not exposed in unprivileged sandboxes.
inline result<std::vector<cpu_common::cache_entry>> cache_descriptors() {
    return fail(errc::not_supported);
}

/// Instruction set features are not exposed through sandboxed queries.
inline result<std::vector<std::string>> instruction_set_features() {
    return fail(errc::not_supported);
}

/// Cumulative processor usage is not exposed without procfs access.
inline result<cpu_common::usage_information> cumulative_processor_usage() {
    return fail(errc::not_supported);
}

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#endif
