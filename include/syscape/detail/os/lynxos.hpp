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
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unistd.h>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

inline result<std::string> parse_version_string(std::string_view content) {
    std::string_view text = content;
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1U);
    }
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n' ||
                             text.back() == ' ' || text.back() == '\t')) {
        text.remove_suffix(1U);
    }
    if (text.empty()) {
        return fail(errc::not_found);
    }
    std::string result_str(text);
    if (!is_valid_utf8(result_str)) {
        return fail(errc::invalid_encoding);
    }
    return result_str;
}

inline result<std::string> parse_host_name(std::string_view content) {
    std::string_view text = content;
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1U);
    }
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n' ||
                             text.back() == ' ' || text.back() == '\t')) {
        text.remove_suffix(1U);
    }
    if (text.empty()) {
        return fail(errc::not_found);
    }
    for (char c : text) {
        if (c == '\0' || c == '\n' || c == '\r') {
            return fail(errc::malformed_data);
        }
    }
    std::string result_str(text);
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
    return std::to_string(value);
}

inline result<std::string> format_version_value(const char* content) {
    if (content == nullptr) {
        return fail(errc::not_found);
    }
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
    if (saved_errno == EINVAL || saved_errno == ENOSYS
#if defined(ENOTSUP)
        || saved_errno == ENOTSUP
#endif
#if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (EOPNOTSUPP != ENOTSUP))
        || saved_errno == EOPNOTSUPP
#endif
    ) {
        return fail(errc::not_supported);
    }
    if (saved_errno != 0) {
        return fail(std::error_code(saved_errno, std::generic_category()));
    }
    return fail(errc::io_error);
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
    std::vector<char> buffer(256U);
    constexpr std::size_t max_buffer_size = 64U * 1024U;
    while (buffer.size() <= max_buffer_size) {
        errno = 0;
        if (::gethostname(buffer.data(), buffer.size()) == 0) {
            std::size_t end = 0U;
            while (end < buffer.size() && buffer[end] != '\0') {
                ++end;
            }
            if (end < buffer.size()) {
                return parse_host_name(std::string_view(buffer.data(), end));
            }
            // Buffer full without null terminator indicates truncation.
        } else {
            const int err = errno;
            if (err == EACCES || err == EPERM) {
                return fail(errc::permission_denied);
            }
            if (err == ENOSYS
#if defined(ENOTSUP)
                || err == ENOTSUP
#endif
#if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (EOPNOTSUPP != ENOTSUP))
                || err == EOPNOTSUPP
#endif
            ) {
                return fail(errc::not_supported);
            }
            if (err != ENAMETOOLONG && err != EINVAL) {
                return fail(std::error_code(err, std::generic_category()));
            }
        }
        if (buffer.size() >= max_buffer_size) {
            return fail(errc::value_too_large);
        }
        buffer.resize(buffer.size() * 2U);
    }
    return fail(errc::value_too_large);
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
