#ifndef SYSCAPE_DETAIL_OS_PIKEOS_HPP
#define SYSCAPE_DETAIL_OS_PIKEOS_HPP

/// @file
/// @brief PikeOS RTOS backend implementation for operating-system queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses <p4.h> when available.

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>

#if !defined(SYSCAPE_PIKEOS_HAS_P4_H)
#if defined(__has_include)
#if __has_include(<p4.h>)
#include <p4.h>
#define SYSCAPE_PIKEOS_HAS_P4_H 1
#elif __has_include("p4.h")
#include "p4.h"
#define SYSCAPE_PIKEOS_HAS_P4_H 1
#endif
#endif
#endif

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
    // When P4_VERSION is passed as a compiler flag -DP4_VERSION without a
    // value, the preprocessor defines it as 1. A bare numeric 1 is a target
    // detection flag rather than a valid PikeOS version number (versions
    // are 3.x, 4.x, 5.x).
    if (value == 1) {
        return fail(errc::not_supported);
    }
    return std::to_string(value);
}

inline result<std::string> format_version_value(const char* content) {
    return parse_version_string(content);
}

inline result<std::string> product_name() {
    return std::string("PikeOS");
}

inline result<std::string> kernel_name() {
    return std::string("PikeOS Microkernel");
}

inline result<std::string> product_version() {
#if defined(SYSCAPE_PIKEOS_VERSION_STRING)
    return format_version_value(SYSCAPE_PIKEOS_VERSION_STRING);
#elif defined(PIKEOS_VERSION_STRING)
    return format_version_value(PIKEOS_VERSION_STRING);
#elif defined(PIKEOS_VERSION)
    return format_version_value(PIKEOS_VERSION);
#elif defined(P4_VERSION)
    return format_version_value(P4_VERSION);
#else
    return fail(errc::not_supported);
#endif
}

namespace os_detail {

#if defined(SYSCAPE_PIKEOS_HAS_P4_H)
template <typename F>
inline auto invoke_p4_get_time(F&& f, int)
    -> decltype(f(), result<std::uint64_t> {}) {
    const auto t = f();
    return static_cast<std::uint64_t>(t);
}

template <typename F>
inline auto invoke_p4_get_time(F&& f, long)
    -> decltype(f(static_cast<P4_time_t*>(nullptr)), result<std::uint64_t> {}) {
    P4_time_t t = 0;
    const auto err = f(&t);
    if (err != 0) {
        return fail(errc::io_error);
    }
    return static_cast<std::uint64_t>(t);
}
#endif

} // namespace os_detail

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_PIKEOS_HAS_P4_H)
    const auto time_res = os_detail::invoke_p4_get_time(::p4_get_time, 0);
    if (!time_res) {
        return fail(time_res.error());
    }
    const std::uint64_t raw_time = *time_res;

#if defined(SYSCAPE_PIKEOS_TICKS_PER_SECOND)
    static_assert(SYSCAPE_PIKEOS_TICKS_PER_SECOND > 0,
                  "SYSCAPE_PIKEOS_TICKS_PER_SECOND must be positive");
    constexpr std::uint64_t ticks_per_sec =
        static_cast<std::uint64_t>(SYSCAPE_PIKEOS_TICKS_PER_SECOND);
    if (raw_time > (std::numeric_limits<std::uint64_t>::max() / 1000ULL)) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t ms = (raw_time * 1000ULL) / ticks_per_sec;
#elif defined(PIKEOS_TICKS_PER_SECOND)
    static_assert(PIKEOS_TICKS_PER_SECOND > 0,
                  "PIKEOS_TICKS_PER_SECOND must be positive");
    constexpr std::uint64_t ticks_per_sec =
        static_cast<std::uint64_t>(PIKEOS_TICKS_PER_SECOND);
    if (raw_time > (std::numeric_limits<std::uint64_t>::max() / 1000ULL)) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t ms = (raw_time * 1000ULL) / ticks_per_sec;
#else
    // Default PikeOS p4_get_time reports nanoseconds.
    const std::uint64_t ms = raw_time / 1000000ULL;
#endif

    if (ms >
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return fail(errc::value_too_large);
    }
    return std::chrono::milliseconds(static_cast<std::int64_t>(ms));
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
    return fail(errc::not_supported);
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

#if defined(SYSCAPE_PIKEOS_HAS_P4_H)
#undef SYSCAPE_PIKEOS_HAS_P4_H
#endif

#endif // SYSCAPE_DETAIL_OS_PIKEOS_HPP
