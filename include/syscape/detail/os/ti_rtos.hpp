#ifndef SYSCAPE_DETAIL_OS_TI_RTOS_HPP
#define SYSCAPE_DETAIL_OS_TI_RTOS_HPP

/// @file
/// @brief TI-RTOS (SYS/BIOS) backend implementation for operating-system
/// queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses <ti/sysbios/BIOS.h> and <ti/sysbios/knl/Clock.h> when available.

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

#if defined(__has_include)
#if __has_include(<ti/sysbios/BIOS.h>)
#include <ti/sysbios/BIOS.h>
#define SYSCAPE_TI_RTOS_HAS_BIOS_H 1
#endif
#if __has_include(<ti/sysbios/knl/Clock.h>)
#include <ti/sysbios/knl/Clock.h>
#define SYSCAPE_TI_RTOS_HAS_CLOCK_H 1
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
#if defined(SYSCAPE_TARGET_SYSBIOS_STANDALONE)
    return std::string("SYS/BIOS");
#else
    return std::string("TI-RTOS");
#endif
}

inline result<std::string> kernel_name() {
    return std::string("SYS/BIOS");
}

inline result<std::string> product_version() {
#if defined(SYSCAPE_TI_RTOS_VERSION_STRING)
    return parse_version_string(SYSCAPE_TI_RTOS_VERSION_STRING);
#elif defined(SYSCAPE_SYSBIOS_VERSION_STRING)
    return parse_version_string(SYSCAPE_SYSBIOS_VERSION_STRING);
#elif defined(SYSCAPE_TI_RTOS_HAS_BIOS_H)
#if defined(ti_sysbios_BIOS_versionStr)
    return parse_version_string(ti_sysbios_BIOS_versionStr);
#elif defined(ti_sysbios_BIOS_version)
    const std::uint32_t ver =
        static_cast<std::uint32_t>(ti_sysbios_BIOS_version);
    if (ver != 0U) {
        const std::uint32_t major = (ver >> 24) & 0xFFU;
        const std::uint32_t minor = (ver >> 16) & 0xFFU;
        const std::uint32_t patch = (ver >> 8) & 0xFFU;
        return std::to_string(major) + "." + std::to_string(minor) + "." +
               std::to_string(patch);
    }
    return fail(errc::not_supported);
#elif defined(BIOS_version)
    const std::uint32_t ver = static_cast<std::uint32_t>(BIOS_version);
    if (ver != 0U) {
        const std::uint32_t major = (ver >> 24) & 0xFFU;
        const std::uint32_t minor = (ver >> 16) & 0xFFU;
        const std::uint32_t patch = (ver >> 8) & 0xFFU;
        return std::to_string(major) + "." + std::to_string(minor) + "." +
               std::to_string(patch);
    }
    return fail(errc::not_supported);
#else
    return fail(errc::not_supported);
#endif
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> kernel_version() {
    return product_version();
}

inline result<std::string> host_name() {
    return fail(errc::not_supported);
}

// Computes monotonic uptime from TI-RTOS (SYS/BIOS) Clock_getTicks() and
// Clock_tickPeriod (tick period in microseconds).
inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_TI_RTOS_HAS_CLOCK_H)
    const auto period_us = static_cast<std::uint64_t>(Clock_tickPeriod);
    if (period_us == 0ULL) {
        return fail(errc::malformed_data);
    }
    const auto ticks = static_cast<std::uint64_t>(Clock_getTicks());
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>((std::chrono::milliseconds::max)().count());
    if (ticks > (std::numeric_limits<std::uint64_t>::max)() / period_us) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t total_us = ticks * period_us;
    const std::uint64_t ms = total_us / 1000ULL;
    if (ms > max_ms) {
        return fail(errc::value_too_large);
    }
    return std::chrono::milliseconds(
        static_cast<std::chrono::milliseconds::rep>(ms));
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::chrono::system_clock::time_point> boot_time() {
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

#if defined(SYSCAPE_TI_RTOS_HAS_BIOS_H)
#undef SYSCAPE_TI_RTOS_HAS_BIOS_H
#endif

#if defined(SYSCAPE_TI_RTOS_HAS_CLOCK_H)
#undef SYSCAPE_TI_RTOS_HAS_CLOCK_H
#endif

#endif // SYSCAPE_DETAIL_OS_TI_RTOS_HPP
