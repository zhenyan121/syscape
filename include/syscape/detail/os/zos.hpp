#ifndef SYSCAPE_DETAIL_OS_ZOS_HPP
#define SYSCAPE_DETAIL_OS_ZOS_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <string>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

/// Returns the z/OS operating system product name.
inline result<std::string> product_name() {
    return std::string("z/OS");
}

/// Returns the z/OS product version if supplied by the SYSCAPE_ZOS_VERSION
/// override macro; otherwise returns not_supported to avoid substituting an
/// unverified version.
inline result<std::string> product_version() {
#if defined(SYSCAPE_ZOS_VERSION)
    return std::string(SYSCAPE_ZOS_VERSION);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

/// Returns the z/OS kernel subsystem name.
inline result<std::string> kernel_name() {
    return std::string("z/OS UNIX System Services");
}

/// Returns the kernel version if supplied by the
/// SYSCAPE_ZOS_KERNEL_VERSION override macro; otherwise returns
/// not_supported to prevent substituting an unverified version.
inline result<std::string> kernel_version() {
#if defined(SYSCAPE_ZOS_KERNEL_VERSION)
    return std::string(SYSCAPE_ZOS_KERNEL_VERSION);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> host_name() {
#if defined(SYSCAPE_ZOS_HOST_NAME)
    return std::string(SYSCAPE_ZOS_HOST_NAME);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_ZOS_UPTIME_MS)
    return std::chrono::milliseconds(SYSCAPE_ZOS_UPTIME_MS);
#else
    return fail(errc::not_supported);
#endif
}

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

inline result<std::string> boot_identifier() {
    return fail(errc::not_supported);
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
