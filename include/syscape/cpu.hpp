#ifndef SYSCAPE_CPU_HPP
#define SYSCAPE_CPU_HPP

/// @file
/// @brief Hosted CPU identity and online topology queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note Linux implements identity and topology. Windows implements topology
/// using Windows 7 or later processor-group APIs. macOS implements logical and
/// physical core counts. Other targets use the not-supported fallback.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/cpu.hpp requires C++17 or later"
#endif

#include <cstdint>
#include <string>
#include <vector>

#include <syscape/detail/cpu/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/cpu/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/cpu/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/cpu/macos.hpp>
#else
#include <syscape/detail/cpu/generic.hpp>
#endif

namespace syscape {
namespace cpu {

/// Returns the distinct CPU vendor identifiers exposed by the platform.
///
/// The result preserves the platform's textual identifiers as UTF-8 and may
/// contain more than one value on a heterogeneous system. An identifier can be
/// a name such as an x86 CPUID vendor string or a numeric code exposed as text;
/// Syscape does not guess a marketing name. Values normally remain unchanged
/// during a process but can change when virtual or hot-pluggable hardware is
/// reconfigured.
/// @return One or more identifiers, not_found when the platform source contains
/// none, not_supported when no acceptable source exists, or a platform I/O,
/// malformed-data, or encoding error.
inline result<std::vector<std::string>> vendor_identifiers() {
    return detail::cpu_common::validate_utf8_labels(
        detail::cpu_backend::vendor_identifiers());
}

/// Returns the distinct CPU model labels exposed by the platform.
///
/// The result preserves the platform's labels as UTF-8 and may contain more
/// than one value on a heterogeneous system. The labels are descriptive, not
/// stable identifiers. They normally remain unchanged during a process but can
/// change when virtual or hot-pluggable hardware is reconfigured.
/// @return One or more labels, not_found when the platform source contains no
/// model label, not_supported when no acceptable source exists, or a platform
/// I/O, malformed-data, or encoding error.
inline result<std::vector<std::string>> model_names() {
    return detail::cpu_common::validate_utf8_labels(
        detail::cpu_backend::model_names());
}

/// Returns the number of logical processors currently online system-wide.
///
/// This count describes processors enabled by the operating system. It is not
/// restricted to the calling process's affinity or container CPU quota and can
/// change while the process is running.
/// @return A positive processor count or not_supported, malformed_data,
/// value_too_large, or a native platform error.
inline result<std::uint32_t> online_logical_processor_count() {
    return detail::cpu_backend::online_logical_processor_count();
}

/// Returns the number of physical CPU cores with an online logical processor.
///
/// This count is system-wide rather than restricted by process affinity and can
/// change when processors are enabled or disabled.
/// @return A positive core count or not_supported when the platform exposes no
/// physical topology, malformed_data, value_too_large, or a native error.
inline result<std::uint32_t> online_physical_core_count() {
    return detail::cpu_backend::online_physical_core_count();
}

/// Returns the number of CPU packages containing an online logical processor.
///
/// A package is a physical processor socket or equivalent package grouping
/// reported by the operating system. The count can change with CPU hot-plug.
/// @return A positive package count or not_supported when the platform exposes
/// no package topology, malformed_data, value_too_large, or a native error.
inline result<std::uint32_t> online_processor_package_count() {
    return detail::cpu_backend::online_processor_package_count();
}

} // namespace cpu
} // namespace syscape

#endif
