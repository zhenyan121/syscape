#ifndef SYSCAPE_DETAIL_OS_EMBOS_HPP
#define SYSCAPE_DETAIL_OS_EMBOS_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<RTOS.h>)
#include <RTOS.h>
#define SYSCAPE_EMBOS_HAS_KERNEL_HEADERS 1
#elif __has_include(<rtos.h>)
#include <rtos.h>
#define SYSCAPE_EMBOS_HAS_KERNEL_HEADERS 1
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
    return std::string("SEGGER embOS");
}

inline result<std::string> product_version() {
#if defined(SYSCAPE_EMBOS_HAS_KERNEL_HEADERS)
    std::uint32_t ver = 0;
#if defined(OS_VERSION)
    ver = static_cast<std::uint32_t>(OS_VERSION);
#endif
    if (ver == 0) {
        ver = static_cast<std::uint32_t>(::OS_GetVersion());
    }
    if (ver == 0) {
        return fail(errc::not_found);
    }
    const std::uint32_t major = ver / 10000;
    const std::uint32_t minor = (ver / 100) % 100;
    const std::uint32_t patch = ver % 100;
    return std::to_string(major) + "." + std::to_string(minor) + "." +
           std::to_string(patch);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

inline result<std::string> kernel_name() {
    return std::string("embOS");
}

inline result<std::string> kernel_version() {
    return product_version();
}

inline result<std::string> host_name() {
    return fail(errc::not_supported);
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_EMBOS_HAS_KERNEL_HEADERS)
#if defined(OS_TICK_FREQ)
    const OS_TIME ticks = ::OS_TIME_GetTicks();
    const auto rate = static_cast<std::uint64_t>(OS_TICK_FREQ);
    if (rate == 0) {
        return fail(errc::malformed_data);
    }
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>(std::chrono::milliseconds::max().count());
    const auto ticks_u64 = static_cast<std::uint64_t>(ticks);

    std::uint64_t ms = 0;
    if (rate == 1000ULL) {
        ms = ticks_u64;
    } else if (ticks_u64 <= UINT64_MAX / 1000ULL) {
        ms = (ticks_u64 * 1000ULL) / rate;
    } else {
        const std::uint64_t q = ticks_u64 / rate;
        const std::uint64_t r = ticks_u64 % rate;
        if (q > max_ms / 1000ULL) {
            return fail(errc::value_too_large);
        }
        ms = q * 1000ULL + (r * 1000ULL) / rate;
    }
    if (ms > max_ms) {
        return fail(errc::value_too_large);
    }
    return std::chrono::milliseconds(
        static_cast<std::chrono::milliseconds::rep>(ms));
#else
    const OS_TIME ticks = ::OS_TIME_GetTicks();
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>(std::chrono::milliseconds::max().count());
    const auto ms =
        static_cast<std::uint64_t>(::OS_TIME_ConvertTicks2ms(ticks));
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

inline result<std::string> boot_identifier() {
    return fail(errc::not_supported);
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#if defined(SYSCAPE_EMBOS_HAS_KERNEL_HEADERS)
#undef SYSCAPE_EMBOS_HAS_KERNEL_HEADERS
#endif

#endif
