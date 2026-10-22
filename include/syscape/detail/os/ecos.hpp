#ifndef SYSCAPE_DETAIL_OS_ECOS_HPP
#define SYSCAPE_DETAIL_OS_ECOS_HPP

/// @file
/// @brief eCos RTOS backend implementation for operating-system queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses <cyg/kernel/kapi.h> when available.

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

#if !defined(SYSCAPE_ECOS_HAS_KAPI_H)
#if defined(__has_include)
#if __has_include(<cyg/kernel/kapi.h>)
#include <cyg/kernel/kapi.h>
#define SYSCAPE_ECOS_HAS_KAPI_H 1
#elif __has_include("cyg/kernel/kapi.h")
#include "cyg/kernel/kapi.h"
#define SYSCAPE_ECOS_HAS_KAPI_H 1
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
    return std::string("eCos");
}

inline result<std::string> kernel_name() {
    return std::string("eCos Kernel");
}

inline result<std::string> product_version() {
#if defined(SYSCAPE_ECOS_VERSION_STRING)
    return parse_version_string(SYSCAPE_ECOS_VERSION_STRING);
#elif defined(CYG_VERSION_STRING)
    return parse_version_string(CYG_VERSION_STRING);
#elif defined(CYG_VERSION_CURRENT)
    return std::to_string(CYG_VERSION_CURRENT);
#elif defined(CYG_VERSION_INIT)
    return std::to_string(CYG_VERSION_INIT);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_ECOS_HAS_KAPI_H)
    const cyg_tick_count_t ticks = cyg_current_time();
#if defined(SYSCAPE_ECOS_TICKS_PER_SECOND) &&                                  \
    (SYSCAPE_ECOS_TICKS_PER_SECOND > 0)
    constexpr std::uint64_t ticks_per_sec = SYSCAPE_ECOS_TICKS_PER_SECOND;
#elif defined(ECOS_TICKS_PER_SECOND) && (ECOS_TICKS_PER_SECOND > 0)
    constexpr std::uint64_t ticks_per_sec = ECOS_TICKS_PER_SECOND;
#elif defined(CYGNUM_HAL_RTC_DENOMINATOR) && (CYGNUM_HAL_RTC_DENOMINATOR > 0)
    constexpr std::uint64_t ticks_per_sec = CYGNUM_HAL_RTC_DENOMINATOR;
#else
    constexpr std::uint64_t ticks_per_sec = 100ULL;
#endif

    static_assert(ticks_per_sec > 0ULL, "eCos tick frequency must be positive");

    if (static_cast<std::uint64_t>(ticks) >
        (std::numeric_limits<std::uint64_t>::max() / 1000ULL)) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t ms =
        (static_cast<std::uint64_t>(ticks) * 1000ULL) / ticks_per_sec;
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

#endif // SYSCAPE_DETAIL_OS_ECOS_HPP
