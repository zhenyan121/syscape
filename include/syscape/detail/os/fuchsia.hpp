#ifndef SYSCAPE_DETAIL_OS_FUCHSIA_HPP
#define SYSCAPE_DETAIL_OS_FUCHSIA_HPP

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <time.h>

#include <sys/utsname.h>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

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

inline result<std::string> product_name() {
    return std::string("Fuchsia");
}

inline result<std::string> product_version() {
    struct utsname uts {};
    if (::uname(&uts) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (uts.release[0] == '\0') {
        return fail(errc::not_found);
    }
    return parse_version_string(uts.release);
}

inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

inline result<std::string> kernel_name() {
    struct utsname uts {};
    if (::uname(&uts) == 0 && uts.sysname[0] != '\0') {
        std::string name(uts.sysname);
        if (!is_valid_utf8(name)) {
            return fail(errc::invalid_encoding);
        }
        return name;
    }
    return std::string("Zircon");
}

inline result<std::string> kernel_version() {
    struct utsname uts {};
    if (::uname(&uts) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (uts.version[0] == '\0') {
        return fail(errc::not_found);
    }
    return parse_version_string(uts.version);
}

inline result<std::string> host_name() {
    struct utsname uts {};
    if (::uname(&uts) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (uts.nodename[0] == '\0') {
        return fail(errc::not_found);
    }
    return parse_host_name(uts.nodename);
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
    if (saved_errno == EINVAL) {
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
