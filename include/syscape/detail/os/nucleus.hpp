#ifndef SYSCAPE_DETAIL_OS_NUCLEUS_HPP
#define SYSCAPE_DETAIL_OS_NUCLEUS_HPP

/// @file
/// @brief Nucleus RTOS backend implementation for operating-system queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses <nucleus.h> when available.

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

#if !defined(SYSCAPE_NUCLEUS_HAS_NUCLEUS_H)
#if defined(__has_include)
#if __has_include(<nucleus.h>)
#include <nucleus.h>
#define SYSCAPE_NUCLEUS_HAS_NUCLEUS_H 1
#elif __has_include("nucleus.h")
#include "nucleus.h"
#define SYSCAPE_NUCLEUS_HAS_NUCLEUS_H 1
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

inline result<std::string> product_name() {
    return std::string("Nucleus RTOS");
}

inline result<std::string> kernel_name() {
    return std::string("Nucleus PLUS");
}

inline result<std::string> product_version() {
#if defined(SYSCAPE_NUCLEUS_VERSION_STRING)
    return parse_version_string(SYSCAPE_NUCLEUS_VERSION_STRING);
#elif defined(SYSCAPE_NUCLEUS_HAS_NUCLEUS_H)
    const char* const release_info = NU_Release_Information();
    if (release_info != nullptr) {
        auto ver = parse_version_string(release_info);
        if (ver) {
            return ver;
        }
    }
#if defined(PLUS_VERSION_NUMBER)
    return std::to_string(PLUS_VERSION_NUMBER);
#else
    return fail(errc::not_supported);
#endif
#elif defined(PLUS_VERSION_NUMBER)
    return std::to_string(PLUS_VERSION_NUMBER);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_NUCLEUS_HAS_NUCLEUS_H)
    const std::uint32_t ticks = NU_Get_Time_Stamp();
#if defined(SYSCAPE_NUCLEUS_TICKS_PER_SECOND) &&                               \
    (SYSCAPE_NUCLEUS_TICKS_PER_SECOND > 0)
    constexpr std::uint64_t ticks_per_sec = SYSCAPE_NUCLEUS_TICKS_PER_SECOND;
#elif defined(NU_PLUS_TICKS_PER_SEC) && (NU_PLUS_TICKS_PER_SEC > 0)
    constexpr std::uint64_t ticks_per_sec = NU_PLUS_TICKS_PER_SEC;
#elif defined(TICKS_PER_SECOND) && (TICKS_PER_SECOND > 0)
    constexpr std::uint64_t ticks_per_sec = TICKS_PER_SECOND;
#else
    constexpr std::uint64_t ticks_per_sec = 100ULL;
#endif

    static_assert(ticks_per_sec > 0ULL,
                  "Nucleus tick frequency must be positive");

    // Step 1: Prevent 64-bit unsigned integer overflow when multiplying ticks
    // by 1000.
    if (static_cast<std::uint64_t>(ticks) >
        (std::numeric_limits<std::uint64_t>::max() / 1000ULL)) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t ms =
        (static_cast<std::uint64_t>(ticks) * 1000ULL) / ticks_per_sec;
    // Step 2: Ensure elapsed milliseconds fit into signed
    // std::chrono::milliseconds rep (int64_t).
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

#endif // SYSCAPE_DETAIL_OS_NUCLEUS_HPP
