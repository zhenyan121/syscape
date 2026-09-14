#ifndef SYSCAPE_DETAIL_OS_CHIBIOS_HPP
#define SYSCAPE_DETAIL_OS_CHIBIOS_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <limits>
#include <string>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<ch.h>)
#include <ch.h>
#define SYSCAPE_CHIBIOS_HAS_KERNEL_HEADERS 1
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
#if defined(__CHIBIOS_NIL__)
    return std::string("ChibiOS/NIL");
#else
    return std::string("ChibiOS/RT");
#endif
}

inline result<std::string> kernel_name() {
#if defined(__CHIBIOS_NIL__)
    return std::string("ChibiOS/NIL");
#else
    return std::string("ChibiOS/RT");
#endif
}

namespace detail {

inline result<std::string> format_chibios_version_numbers(std::uint32_t major,
                                                          std::uint32_t minor,
                                                          std::uint32_t patch) {
    return std::to_string(major) + "." + std::to_string(minor) + "." +
           std::to_string(patch);
}

} // namespace detail

inline result<std::string> product_version() {
#if defined(CH_KERNEL_VERSION)
    return std::string(CH_KERNEL_VERSION);
#elif defined(CH_KERNEL_MAJOR) && defined(CH_KERNEL_MINOR) &&                  \
    defined(CH_KERNEL_PATCH)
    return detail::format_chibios_version_numbers(
        static_cast<std::uint32_t>(CH_KERNEL_MAJOR),
        static_cast<std::uint32_t>(CH_KERNEL_MINOR),
        static_cast<std::uint32_t>(CH_KERNEL_PATCH));
#elif defined(CH_KERNEL_MAJOR) && defined(CH_KERNEL_MINOR)
    return detail::format_chibios_version_numbers(
        static_cast<std::uint32_t>(CH_KERNEL_MAJOR),
        static_cast<std::uint32_t>(CH_KERNEL_MINOR), 0U);
#elif defined(CH_VERSION)
    return std::string(CH_VERSION);
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
#if defined(SYSCAPE_CHIBIOS_HAS_KERNEL_HEADERS)
#if defined(CH_CFG_ST_FREQUENCY)
    const std::uint64_t freq = static_cast<std::uint64_t>(CH_CFG_ST_FREQUENCY);
#elif defined(CH_ST_FREQUENCY)
    const std::uint64_t freq = static_cast<std::uint64_t>(CH_ST_FREQUENCY);
#elif defined(CH_FREQUENCY)
    const std::uint64_t freq = static_cast<std::uint64_t>(CH_FREQUENCY);
#else
    return fail(errc::not_supported);
#endif

#if defined(CH_CFG_ST_FREQUENCY) || defined(CH_ST_FREQUENCY) ||                \
    defined(CH_FREQUENCY)
    if (freq == 0ULL) {
        return fail(errc::malformed_data);
    }
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>(std::chrono::milliseconds::max().count());
    const std::uint64_t ticks =
        static_cast<std::uint64_t>(chVTGetSystemTimeX());
    std::uint64_t ms = 0;
    if (freq == 1000ULL) {
        ms = ticks;
    } else if (ticks <= UINT64_MAX / 1000ULL) {
        ms = (ticks * 1000ULL) / freq;
    } else {
        const std::uint64_t q = ticks / freq;
        const std::uint64_t r = ticks % freq;
        if (q > max_ms / 1000ULL) {
            return fail(errc::value_too_large);
        }
        ms = q * 1000ULL + (r * 1000ULL) / freq;
    }
    if (ms > max_ms) {
        return fail(errc::value_too_large);
    }
    return std::chrono::milliseconds(
        static_cast<std::chrono::milliseconds::rep>(ms));
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

#if defined(SYSCAPE_CHIBIOS_HAS_KERNEL_HEADERS)
#undef SYSCAPE_CHIBIOS_HAS_KERNEL_HEADERS
#endif

#endif
