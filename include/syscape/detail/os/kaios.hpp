#ifndef SYSCAPE_DETAIL_OS_KAIOS_HPP
#define SYSCAPE_DETAIL_OS_KAIOS_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <string>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

/// Returns the KaiOS operating system product name.
inline result<std::string> product_name() {
    return std::string("KaiOS");
}

/// Returns the KaiOS product version if supplied by override macro.
inline result<std::string> product_version() {
#if defined(SYSCAPE_KAIOS_VERSION)
    return std::string(SYSCAPE_KAIOS_VERSION);
#else
    return fail(errc::not_supported);
#endif
}

/// Build identifier query is not supported in unprivileged sandboxes.
inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

/// Returns the kernel name ("Linux" by default, or override macro).
inline result<std::string> kernel_name() {
#if defined(SYSCAPE_KAIOS_KERNEL_NAME)
    return std::string(SYSCAPE_KAIOS_KERNEL_NAME);
#else
    return std::string("Linux");
#endif
}

/// Returns the kernel version if supplied by override macro.
inline result<std::string> kernel_version() {
#if defined(SYSCAPE_KAIOS_KERNEL_VERSION)
    return std::string(SYSCAPE_KAIOS_KERNEL_VERSION);
#else
    return fail(errc::not_supported);
#endif
}

/// Returns the host name if supplied by override macro.
inline result<std::string> host_name() {
#if defined(SYSCAPE_KAIOS_HOST_NAME)
    return std::string(SYSCAPE_KAIOS_HOST_NAME);
#else
    return fail(errc::not_supported);
#endif
}

/// Returns the system uptime in milliseconds if supplied by override macro.
inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_KAIOS_UPTIME_MS)
    return std::chrono::milliseconds(SYSCAPE_KAIOS_UPTIME_MS);
#else
    return fail(errc::not_supported);
#endif
}

/// Computes the system boot time from the uptime snapshot.
inline result<std::chrono::system_clock::time_point> boot_time() {
    const auto up = uptime();
    if (!up) {
        return fail(up.error());
    }
    const auto now = std::chrono::system_clock::now();
    const auto duration =
        std::chrono::duration_cast<std::chrono::system_clock::duration>(*up);
    if (duration > now.time_since_epoch()) {
        return fail(errc::malformed_data);
    }
    return now - duration;
}

/// Boot identifier is not exposed without privileged root or sysfs access.
inline result<std::string> boot_identifier() {
    return fail(errc::not_supported);
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
