#ifndef SYSCAPE_DETAIL_OS_RIOT_HPP
#define SYSCAPE_DETAIL_OS_RIOT_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <limits>
#include <string>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<kernel_defines.h>)
#include <kernel_defines.h>
#define SYSCAPE_RIOT_HAS_KERNEL_HEADERS 1
#endif
#if __has_include(<riot_version.h>)
#include <riot_version.h>
#define SYSCAPE_RIOT_HAS_KERNEL_HEADERS 1
#elif __has_include(<version.h>)
#include <version.h>
#define SYSCAPE_RIOT_HAS_KERNEL_HEADERS 1
#endif
#if __has_include(<ztimer64.h>)
#include <ztimer64.h>
#define SYSCAPE_RIOT_HAS_KERNEL_HEADERS 1
#endif
#if __has_include(<ztimer.h>)
#include <ztimer.h>
#define SYSCAPE_RIOT_HAS_KERNEL_HEADERS 1
#endif
#if __has_include(<xtimer.h>)
#include <xtimer.h>
#define SYSCAPE_RIOT_HAS_KERNEL_HEADERS 1
#define SYSCAPE_RIOT_HAS_XTIMER 1
#endif
#endif
#if defined(__cplusplus)
}
#endif

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

inline result<std::string> product_name() {
    return std::string("RIOT");
}

inline result<std::string> kernel_name() {
    return std::string("RIOT");
}

inline result<std::string> product_version() {
#if defined(RIOT_VERSION)
    return std::string(RIOT_VERSION);
#elif defined(RIOT_VERSION_STRING)
    return std::string(RIOT_VERSION_STRING);
#elif defined(RIOT_VERSION_CODE)
    return std::to_string(static_cast<std::uint32_t>(RIOT_VERSION_CODE));
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

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_RIOT_HAS_KERNEL_HEADERS)
#if defined(RIOT_NO_TIMER)
    return fail(errc::not_supported);
#elif defined(RIOT_USE_XTIMER)
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>(std::chrono::milliseconds::max().count());
    const std::uint64_t usec = static_cast<std::uint64_t>(xtimer_now_usec64());
    const std::uint64_t ms = usec / 1000ULL;
    if (ms > max_ms) {
        return fail(errc::value_too_large);
    }
    return std::chrono::milliseconds(
        static_cast<std::chrono::milliseconds::rep>(ms));
#elif defined(RIOT_USE_ZTIMER32)
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>(std::chrono::milliseconds::max().count());
    const std::uint64_t ms =
        static_cast<std::uint64_t>(ztimer_now(ZTIMER_MSEC));
    if (ms > max_ms) {
        return fail(errc::value_too_large);
    }
    return std::chrono::milliseconds(
        static_cast<std::chrono::milliseconds::rep>(ms));
#else
#if defined(ZTIMER64_MSEC)
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>(std::chrono::milliseconds::max().count());
    const std::uint64_t ms =
        static_cast<std::uint64_t>(ztimer64_now(ZTIMER64_MSEC));
    if (ms > max_ms) {
        return fail(errc::value_too_large);
    }
    return std::chrono::milliseconds(
        static_cast<std::chrono::milliseconds::rep>(ms));
#elif defined(ZTIMER_MSEC)
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>(std::chrono::milliseconds::max().count());
    const std::uint64_t ms =
        static_cast<std::uint64_t>(ztimer_now(ZTIMER_MSEC));
    if (ms > max_ms) {
        return fail(errc::value_too_large);
    }
    return std::chrono::milliseconds(
        static_cast<std::chrono::milliseconds::rep>(ms));
#elif defined(MODULE_XTIMER) || defined(XTIMER_H) ||                           \
    defined(SYSCAPE_RIOT_HAS_XTIMER)
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>(std::chrono::milliseconds::max().count());
    const std::uint64_t usec = static_cast<std::uint64_t>(xtimer_now_usec64());
    const std::uint64_t ms = usec / 1000ULL;
    if (ms > max_ms) {
        return fail(errc::value_too_large);
    }
    return std::chrono::milliseconds(
        static_cast<std::chrono::milliseconds::rep>(ms));
#else
    return fail(errc::not_supported);
#endif
#endif
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

#if defined(SYSCAPE_RIOT_HAS_XTIMER)
#undef SYSCAPE_RIOT_HAS_XTIMER
#endif

#if defined(SYSCAPE_RIOT_HAS_KERNEL_HEADERS)
#undef SYSCAPE_RIOT_HAS_KERNEL_HEADERS
#endif

#endif
