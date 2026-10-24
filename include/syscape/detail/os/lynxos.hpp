#ifndef SYSCAPE_DETAIL_OS_LYNXOS_HPP
#define SYSCAPE_DETAIL_OS_LYNXOS_HPP

/// @file
/// @brief LynxOS / LynxOS-178 RTOS backend implementation for operating-system
/// queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <string>
#include <type_traits>
#include <unistd.h>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

inline result<std::string> parse_version_string(const char* content) {
    if (content == nullptr) {
        return fail(errc::not_found);
    }
    const char* text = content;
    while (*text == ' ' || *text == '\t') {
        ++text;
    }
    if (*text == '\0') {
        return fail(errc::not_found);
    }
    const char* end = text + std::strlen(text);
    while (end > text && (*(end - 1) == '\r' || *(end - 1) == '\n' ||
                          *(end - 1) == ' ' || *(end - 1) == '\t')) {
        --end;
    }
    if (end == text) {
        return fail(errc::not_found);
    }
    std::string result_str(text, static_cast<std::size_t>(end - text));
    if (!is_valid_utf8(result_str)) {
        return fail(errc::invalid_encoding);
    }
    return result_str;
}

template <typename T>
inline auto format_version_value(T value) ->
    typename std::enable_if<std::is_integral<T>::value,
                            result<std::string>>::type {
    if (value <= 0) {
        return fail(errc::not_found);
    }
    // Bare numeric 1 is a feature-test macro rather than a genuine version.
    if (value == 1) {
        return fail(errc::not_supported);
    }
    return std::to_string(value);
}

inline result<std::string> format_version_value(const char* content) {
    return parse_version_string(content);
}

inline result<std::string> product_name() {
#if defined(LYNXOS_178) || defined(__LYNXOS_178__)
    return std::string("LynxOS-178");
#else
    return std::string("LynxOS");
#endif
}

inline result<std::string> kernel_name() {
#if defined(LYNXOS_178) || defined(__LYNXOS_178__)
    return std::string("LynxOS-178 Microkernel");
#else
    return std::string("LynxOS Kernel");
#endif
}

inline result<std::string> product_version() {
#if defined(SYSCAPE_LYNXOS_VERSION_STRING)
    return format_version_value(SYSCAPE_LYNXOS_VERSION_STRING);
#elif defined(LYNXOS_VERSION_STRING)
    return format_version_value(LYNXOS_VERSION_STRING);
#elif defined(LYNXOS_VERSION)
    return format_version_value(LYNXOS_VERSION);
#elif defined(_LYNXOS_VERSION)
    return format_version_value(_LYNXOS_VERSION);
#elif defined(__LYNXOS_VERSION__)
    return format_version_value(__LYNXOS_VERSION__);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(CLOCK_MONOTONIC)
    struct timespec ts {};
    if (::clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        if (ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000L) {
            return fail(errc::malformed_data);
        }
        const auto seconds = static_cast<std::uint64_t>(ts.tv_sec);
        const auto maximum_milliseconds = static_cast<std::uint64_t>(
            (std::chrono::milliseconds::max)().count());
        if (seconds > maximum_milliseconds / 1000U) {
            return fail(errc::value_too_large);
        }
        const auto milliseconds =
            seconds * 1000U + static_cast<std::uint64_t>(ts.tv_nsec / 1000000L);
        if (milliseconds > maximum_milliseconds) {
            return fail(errc::value_too_large);
        }
        return std::chrono::milliseconds(
            static_cast<std::chrono::milliseconds::rep>(milliseconds));
    }
    const int saved_errno = errno;
    if (saved_errno == EACCES || saved_errno == EPERM) {
        return fail(errc::permission_denied);
    }
#endif
#if defined(SYSCAPE_LYNXOS_TICKS_PER_SECOND)
    static_assert(SYSCAPE_LYNXOS_TICKS_PER_SECOND > 0,
                  "SYSCAPE_LYNXOS_TICKS_PER_SECOND must be positive");
    return fail(errc::not_supported);
#elif defined(LYNXOS_TICKS_PER_SECOND)
    static_assert(LYNXOS_TICKS_PER_SECOND > 0,
                  "LYNXOS_TICKS_PER_SECOND must be positive");
    return fail(errc::not_supported);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::chrono::system_clock::time_point> boot_time() {
    return fail(errc::not_supported);
}

inline result<std::string> kernel_version() {
    return product_version();
}

inline result<std::string> host_name() {
#if defined(_SC_HOST_NAME_MAX) || defined(HOST_NAME_MAX)
    char buf[256];
    if (::gethostname(buf, sizeof(buf)) == 0) {
        buf[sizeof(buf) - 1] = '\0';
        return parse_version_string(buf);
    }
    const int saved_errno = errno;
    if (saved_errno == EACCES || saved_errno == EPERM) {
        return fail(errc::permission_denied);
    }
    return fail(errc::io_error);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

inline result<std::string> boot_identifier() {
    return fail(errc::not_supported);
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_OS_LYNXOS_HPP
