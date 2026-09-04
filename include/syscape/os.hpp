#ifndef SYSCAPE_OS_HPP
#define SYSCAPE_OS_HPP

/// @file
/// @brief Hosted operating-system identity and boot information queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (Sandboxed/Restricted on Apple mobile platforms, Android, and OpenHarmony).
/// @note Linux, Windows, macOS, Apple mobile platforms (iOS, iPadOS, tvOS,
/// watchOS, visionOS, and Mac Catalyst), Android, OpenHarmony, FreeBSD,
/// OpenBSD, NetBSD, DragonFly BSD, Solaris, and Haiku have native backends.
/// Haiku implements OS identity and boot queries through uname,
/// get_system_info, and system_time. Other targets use the generic
/// not-supported fallback.
/// @note Expected failures are returned as native error codes where available,
/// or as syscape::errc values for missing, malformed, or unsupported data.
/// @note The Windows backend requires Windows Vista or later SDK declarations.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/os.hpp requires C++17 or later"
#endif

#include <chrono>
#include <string>

#include <syscape/detail/os/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) &&           \
    !defined(__ANDROID__) && !defined(SYSCAPE_TARGET_OPENHARMONY)
#include <syscape/detail/os/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/os/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_APPLE_MOBILE)
#include <syscape/detail/os/apple_mobile.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MACOS)
#include <syscape/detail/os/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/os/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/os/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/os/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/os/dragonfly.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__ANDROID__)
#include <syscape/detail/os/android.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_OPENHARMONY)
#include <syscape/detail/os/openharmony.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    (defined(__sun) || defined(__sun__) || defined(sun))
#include <syscape/detail/os/solaris.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__HAIKU__)
#include <syscape/detail/os/haiku.hpp>
#else
#include <syscape/detail/os/generic.hpp>
#endif

namespace syscape {
namespace os {

/// Returns the user-facing operating-system product name as UTF-8.
/// @return The name, not_found when the platform source omits it, or an error.
inline result<std::string> product_name() {
    return detail::os_common::validate_utf8(detail::os_backend::product_name());
}

/// Returns the operating-system product version as UTF-8.
/// @return The version, not_found when absent, or a native platform error.
inline result<std::string> product_version() {
    return detail::os_common::validate_utf8(detail::os_backend::product_version());
}

/// Returns the operating-system build identifier as UTF-8.
/// @return The build identifier, not_found when absent, or a platform error.
inline result<std::string> build_identifier() {
    return detail::os_common::validate_utf8(detail::os_backend::build_identifier());
}

/// Returns the kernel family name as UTF-8.
/// @return The name or a native platform error.
inline result<std::string> kernel_name() {
    return detail::os_common::validate_utf8(detail::os_backend::kernel_name());
}

/// Returns the running kernel version as UTF-8.
/// @return The version or a native platform error.
inline result<std::string> kernel_version() {
    return detail::os_common::validate_utf8(detail::os_backend::kernel_version());
}

/// Returns the local host name as UTF-8.
/// @return The name or a native platform error such as permission denied.
inline result<std::string> host_name() {
    return detail::os_common::validate_utf8(detail::os_backend::host_name());
}

/// Returns an identifier that remains stable only for the current boot.
/// @return The identifier or not_supported when the platform exposes none.
inline result<std::string> boot_identifier() {
    return detail::os_common::validate_utf8(detail::os_backend::boot_identifier());
}

/// Returns elapsed milliseconds since system boot, including suspended time
/// where the platform exposes that meaning.
/// @return A nonnegative duration or a native timing error.
inline result<std::chrono::milliseconds> uptime() {
    return detail::os_backend::uptime();
}

/// Returns the best available system-clock time point at which the system
/// booted.
///
/// A wall-clock correction after boot can make this historical instant later
/// than the current system-clock time. Use uptime() for an elapsed-duration
/// calculation that is independent of wall-clock adjustments.
/// @return The boot time or an error when the platform source is unavailable.
inline result<std::chrono::system_clock::time_point> boot_time() {
    return detail::os_backend::boot_time();
}

} // namespace os
} // namespace syscape

#endif
