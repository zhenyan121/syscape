#ifndef SYSCAPE_DETAIL_OS_MBED_HPP
#define SYSCAPE_DETAIL_OS_MBED_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <limits>
#include <string>

#if defined(__has_include)
#if __has_include(<mbed.h>)
#include <mbed.h>
#define SYSCAPE_MBED_HAS_KERNEL_HEADERS 1
#elif __has_include(<cmsis_os2.h>)
#include <cmsis_os2.h>
#define SYSCAPE_MBED_HAS_KERNEL_HEADERS 1
#elif __has_include(<mbed_version.h>)
#include <mbed_version.h>
#define SYSCAPE_MBED_HAS_KERNEL_HEADERS 1
#endif
#endif

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

inline result<std::string> product_name() {
    return std::string("Arm Mbed OS");
}

inline result<std::string> kernel_name() {
    return std::string("Mbed OS");
}

namespace detail {

inline result<std::string> format_mbed_version_numbers(std::uint32_t major,
                                                       std::uint32_t minor,
                                                       std::uint32_t patch) {
    return std::to_string(major) + "." + std::to_string(minor) + "." +
           std::to_string(patch);
}

} // namespace detail

inline result<std::string> product_version() {
#if defined(MBED_VERSION_STRING)
    return std::string(MBED_VERSION_STRING);
#elif defined(MBED_MAJOR_VERSION) && defined(MBED_MINOR_VERSION) &&            \
    defined(MBED_PATCH_VERSION)
    return detail::format_mbed_version_numbers(
        static_cast<std::uint32_t>(MBED_MAJOR_VERSION),
        static_cast<std::uint32_t>(MBED_MINOR_VERSION),
        static_cast<std::uint32_t>(MBED_PATCH_VERSION));
#elif defined(MBED_MAJOR_VERSION) && defined(MBED_MINOR_VERSION)
    return detail::format_mbed_version_numbers(
        static_cast<std::uint32_t>(MBED_MAJOR_VERSION),
        static_cast<std::uint32_t>(MBED_MINOR_VERSION), 0U);
#elif defined(MBED_VERSION)
    return std::to_string(static_cast<std::uint32_t>(MBED_VERSION));
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
#if defined(SYSCAPE_MBED_HAS_KERNEL_HEADERS)
#if defined(MBED_NO_TICK_FREQ)
    return fail(errc::not_supported);
#else
#if defined(MBED_CONF_RTOS_TICK_FREQ) && (MBED_CONF_RTOS_TICK_FREQ > 0)
    const std::uint64_t freq =
        static_cast<std::uint64_t>(MBED_CONF_RTOS_TICK_FREQ);
#elif defined(OS_TICK_FREQ) && (OS_TICK_FREQ > 0)
    const std::uint64_t freq = static_cast<std::uint64_t>(OS_TICK_FREQ);
#elif defined(MBED_TICK_FREQ) && (MBED_TICK_FREQ > 0)
    const std::uint64_t freq = static_cast<std::uint64_t>(MBED_TICK_FREQ);
#else
    const std::uint64_t freq =
        static_cast<std::uint64_t>(osKernelGetTickFreq());
#endif

    if (freq == 0ULL) {
        return fail(errc::malformed_data);
    }
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>(std::chrono::milliseconds::max().count());
    const std::uint64_t ticks =
        static_cast<std::uint64_t>(osKernelGetTickCount());
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

#if defined(SYSCAPE_MBED_HAS_KERNEL_HEADERS)
#undef SYSCAPE_MBED_HAS_KERNEL_HEADERS
#endif

#endif
