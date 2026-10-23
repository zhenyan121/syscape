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

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_PIKEOS_HAS_P4_H)
    P4_time_t raw_time = 0;
    const P4_e_t status = p4_get_time(&raw_time);
    if (status != 0) {
        return fail(errc::io_error);
    }

#if defined(SYSCAPE_PIKEOS_TICKS_PER_SECOND)
    static_assert(SYSCAPE_PIKEOS_TICKS_PER_SECOND > 0,
                  "SYSCAPE_PIKEOS_TICKS_PER_SECOND must be positive");
    constexpr std::uint64_t ticks_per_sec =
        static_cast<std::uint64_t>(SYSCAPE_PIKEOS_TICKS_PER_SECOND);
    const std::uint64_t ticks = static_cast<std::uint64_t>(raw_time);
    if (ticks > (std::numeric_limits<std::uint64_t>::max() / 1000ULL)) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t ms = (ticks * 1000ULL) / ticks_per_sec;
#elif defined(PIKEOS_TICKS_PER_SECOND)
    static_assert(PIKEOS_TICKS_PER_SECOND > 0,
                  "PIKEOS_TICKS_PER_SECOND must be positive");
    constexpr std::uint64_t ticks_per_sec =
        static_cast<std::uint64_t>(PIKEOS_TICKS_PER_SECOND);
    const std::uint64_t ticks = static_cast<std::uint64_t>(raw_time);
    if (ticks > (std::numeric_limits<std::uint64_t>::max() / 1000ULL)) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t ms = (ticks * 1000ULL) / ticks_per_sec;
#else
    // Default PikeOS p4_get_time reports nanoseconds.
    const std::uint64_t ns = static_cast<std::uint64_t>(raw_time);
    const std::uint64_t ms = ns / 1000000ULL;
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

#endif // SYSCAPE_DETAIL_OS_PIKEOS_HPP
