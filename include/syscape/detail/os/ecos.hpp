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
#elif defined(ECOS_VERSION_STRING)
    return parse_version_string(ECOS_VERSION_STRING);
#elif defined(CYG_VERSION_STRING)
    return parse_version_string(CYG_VERSION_STRING);
#else
    return fail(errc::not_supported);
#endif
}

// Greatest common divisor helper for reducing RTC clock tick fraction.
constexpr std::uint64_t rtc_gcd(std::uint64_t a, std::uint64_t b) noexcept {
    while (b != 0ULL) {
        const std::uint64_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_ECOS_HAS_KAPI_H)
#if defined(SYSCAPE_ECOS_TICKS_PER_SECOND) ||                                  \
    defined(ECOS_TICKS_PER_SECOND) ||                                          \
    (defined(CYGNUM_HAL_RTC_NUMERATOR) && defined(CYGNUM_HAL_RTC_DENOMINATOR))
    const cyg_tick_count_t raw_ticks = cyg_current_time();
    const std::uint64_t ticks = static_cast<std::uint64_t>(raw_ticks);

#if defined(SYSCAPE_ECOS_TICKS_PER_SECOND)
    static_assert(SYSCAPE_ECOS_TICKS_PER_SECOND > 0,
                  "SYSCAPE_ECOS_TICKS_PER_SECOND must be positive");
    constexpr std::uint64_t ticks_per_sec =
        static_cast<std::uint64_t>(SYSCAPE_ECOS_TICKS_PER_SECOND);
    if (ticks > (std::numeric_limits<std::uint64_t>::max() / 1000ULL)) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t ms = (ticks * 1000ULL) / ticks_per_sec;
#elif defined(ECOS_TICKS_PER_SECOND)
    static_assert(ECOS_TICKS_PER_SECOND > 0,
                  "ECOS_TICKS_PER_SECOND must be positive");
    constexpr std::uint64_t ticks_per_sec =
        static_cast<std::uint64_t>(ECOS_TICKS_PER_SECOND);
    if (ticks > (std::numeric_limits<std::uint64_t>::max() / 1000ULL)) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t ms = (ticks * 1000ULL) / ticks_per_sec;
#elif defined(CYGNUM_HAL_RTC_NUMERATOR) && defined(CYGNUM_HAL_RTC_DENOMINATOR)
    static_assert(CYGNUM_HAL_RTC_NUMERATOR > 0ULL,
                  "CYGNUM_HAL_RTC_NUMERATOR must be positive");
    static_assert(CYGNUM_HAL_RTC_DENOMINATOR > 0ULL,
                  "CYGNUM_HAL_RTC_DENOMINATOR must be positive");
    // In eCos, tick period in nanoseconds is (NUMERATOR / DENOMINATOR).
    // Total nanoseconds = (ticks * NUMERATOR) / DENOMINATOR.
    // Total milliseconds = total nanoseconds / 1,000,000
    //                    = (ticks * NUMERATOR) / (DENOMINATOR * 1,000,000).
    // Reduce fraction (NUMERATOR / 1,000,000) using GCD to prevent 64-bit
    // overflow.
    constexpr std::uint64_t num_val =
        static_cast<std::uint64_t>(CYGNUM_HAL_RTC_NUMERATOR);
    constexpr std::uint64_t den_val =
        static_cast<std::uint64_t>(CYGNUM_HAL_RTC_DENOMINATOR);
    constexpr std::uint64_t g = rtc_gcd(num_val, 1000000ULL);
    constexpr std::uint64_t num_reduced = num_val / g;
    constexpr std::uint64_t ns_to_ms_reduced = 1000000ULL / g;

    static_assert(den_val <= (std::numeric_limits<std::uint64_t>::max() /
                              ns_to_ms_reduced),
                  "eCos RTC denominator times scale factor exceeds uint64_t");
    constexpr std::uint64_t effective_denominator = den_val * ns_to_ms_reduced;

    if (num_reduced > 0ULL &&
        ticks > (std::numeric_limits<std::uint64_t>::max() / num_reduced)) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t ms = (ticks * num_reduced) / effective_denominator;
#endif

    if (ms >
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return fail(errc::value_too_large);
    }
    return std::chrono::milliseconds(static_cast<std::int64_t>(ms));
#else
    return fail(errc::not_supported);
#endif
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
