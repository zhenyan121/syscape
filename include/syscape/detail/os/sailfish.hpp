#ifndef SYSCAPE_DETAIL_OS_SAILFISH_HPP
#define SYSCAPE_DETAIL_OS_SAILFISH_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

template <typename T>
inline result<std::chrono::milliseconds>
validate_non_negative_uptime_ms(T value) {
    if constexpr (std::is_signed<T>::value) {
        if (value < 0) {
            return fail(errc::malformed_data);
        }
    }
    if constexpr (!std::is_signed<T>::value) {
        if (static_cast<std::uint64_t>(value) >
            static_cast<std::uint64_t>(
                (std::numeric_limits<std::int64_t>::max)())) {
            return fail(errc::value_too_large);
        }
    }
    return std::chrono::milliseconds(static_cast<std::int64_t>(value));
}

/// Returns the Sailfish OS operating system product name.
inline result<std::string> product_name() {
    return std::string("Sailfish OS");
}

/// Returns the Sailfish OS product version if supplied by override macro.
inline result<std::string> product_version() {
#if defined(SYSCAPE_SAILFISH_VERSION)
    return std::string(SYSCAPE_SAILFISH_VERSION);
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
#if defined(SYSCAPE_SAILFISH_KERNEL_NAME)
    return std::string(SYSCAPE_SAILFISH_KERNEL_NAME);
#else
    return std::string("Linux");
#endif
}

/// Returns the kernel version if supplied by override macro.
inline result<std::string> kernel_version() {
#if defined(SYSCAPE_SAILFISH_KERNEL_VERSION)
    return std::string(SYSCAPE_SAILFISH_KERNEL_VERSION);
#else
    return fail(errc::not_supported);
#endif
}

/// Returns the host name if supplied by override macro.
inline result<std::string> host_name() {
#if defined(SYSCAPE_SAILFISH_HOST_NAME)
    return std::string(SYSCAPE_SAILFISH_HOST_NAME);
#else
    return fail(errc::not_supported);
#endif
}

/// Returns the system uptime in milliseconds if supplied by override macro.
inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_SAILFISH_UPTIME_MS)
    return validate_non_negative_uptime_ms(SYSCAPE_SAILFISH_UPTIME_MS);
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
