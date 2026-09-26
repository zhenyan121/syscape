#ifndef SYSCAPE_DETAIL_OS_RTTHREAD_HPP
#define SYSCAPE_DETAIL_OS_RTTHREAD_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>

#if !defined(SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS)
#define SYSCAPE_RTTHREAD_INTERNAL_KERNEL_HEADERS 1
#endif

#if defined(__has_include)
#if __has_include(<rtthread.h>)
#include <rtthread.h>
#define SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS 1
#elif __has_include(<rtdef.h>)
#include <rtdef.h>
#define SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS 1
#endif
#endif

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

inline result<std::string> product_name() {
    return std::string("RT-Thread");
}

inline result<std::string> kernel_name() {
    return std::string("RT-Thread");
}

inline result<std::string> product_version() {
#if defined(RT_VERSION_STRING)
    return std::string(RT_VERSION_STRING);
#elif defined(RT_VERSION_MAJOR) && defined(RT_VERSION_MINOR) &&                \
    defined(RT_VERSION_PATCH)
    return std::to_string(RT_VERSION_MAJOR) + "." +
           std::to_string(RT_VERSION_MINOR) + "." +
           std::to_string(RT_VERSION_PATCH);
#elif defined(RT_VERSION) && defined(RT_SUBVERSION) && defined(RT_REVISION)
    return std::to_string(RT_VERSION) + "." + std::to_string(RT_SUBVERSION) +
           "." + std::to_string(RT_REVISION);
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
#if defined(SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS)
#if defined(RT_NO_TIMER)
    return fail(errc::not_supported);
#elif defined(RT_TICK_PER_SECOND) && (RT_TICK_PER_SECOND > 0)
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>(std::chrono::milliseconds::max().count());
    const std::uint64_t ticks = static_cast<std::uint64_t>(rt_tick_get());
    const std::uint64_t hz = static_cast<std::uint64_t>(RT_TICK_PER_SECOND);
    if (ticks > max_ms / 1000ULL) {
        const std::uint64_t ms =
            (ticks / hz) * 1000ULL + ((ticks % hz) * 1000ULL) / hz;
        if (ms > max_ms) {
            return fail(errc::value_too_large);
        }
        return std::chrono::milliseconds(
            static_cast<std::chrono::milliseconds::rep>(ms));
    }
    const std::uint64_t ms = (ticks * 1000ULL) / hz;
    if (ms > max_ms) {
        return fail(errc::value_too_large);
    }
    return std::chrono::milliseconds(
        static_cast<std::chrono::milliseconds::rep>(ms));
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

inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

inline result<std::string> boot_identifier() {
    return fail(errc::not_supported);
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#if defined(SYSCAPE_RTTHREAD_INTERNAL_KERNEL_HEADERS)
#undef SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS
#undef SYSCAPE_RTTHREAD_INTERNAL_KERNEL_HEADERS
#endif

#endif
