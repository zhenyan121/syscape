#ifndef SYSCAPE_DETAIL_OS_MYNEWT_HPP
#define SYSCAPE_DETAIL_OS_MYNEWT_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <limits>
#include <string>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<os/os.h>)
#include <os/os.h>
#define SYSCAPE_MYNEWT_HAS_KERNEL_HEADERS 1
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
    return std::string("Apache Mynewt");
}

inline result<std::string> kernel_name() {
    return std::string("Mynewt OS");
}

namespace detail {

inline result<std::string>
format_mynewt_version_numbers(std::uint32_t major, std::uint32_t minor,
                              std::uint32_t revision) {
    return std::to_string(major) + "." + std::to_string(minor) + "." +
           std::to_string(revision);
}

} // namespace detail

inline result<std::string> product_version() {
#if defined(MYNEWT_VERSION_STRING)
    return std::string(MYNEWT_VERSION_STRING);
#elif defined(MYNEWT_VERSION_MAJOR) && defined(MYNEWT_VERSION_MINOR) &&        \
    defined(MYNEWT_VERSION_REVISION)
    return detail::format_mynewt_version_numbers(
        static_cast<std::uint32_t>(MYNEWT_VERSION_MAJOR),
        static_cast<std::uint32_t>(MYNEWT_VERSION_MINOR),
        static_cast<std::uint32_t>(MYNEWT_VERSION_REVISION));
#elif defined(MYNEWT_VERSION_MAJOR) && defined(MYNEWT_VERSION_MINOR)
    return detail::format_mynewt_version_numbers(
        static_cast<std::uint32_t>(MYNEWT_VERSION_MAJOR),
        static_cast<std::uint32_t>(MYNEWT_VERSION_MINOR), 0U);
#elif defined(OS_VERSION)
    return std::string(OS_VERSION);
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
#if defined(SYSCAPE_MYNEWT_HAS_KERNEL_HEADERS)
#if defined(OS_TICKS_PER_SEC)
    const std::uint64_t freq = static_cast<std::uint64_t>(OS_TICKS_PER_SEC);
#elif defined(MYNEWT_VAL) && defined(MYNEWT_VAL_OS_TICKS_PER_SEC)
    const std::uint64_t freq =
        static_cast<std::uint64_t>(MYNEWT_VAL(OS_TICKS_PER_SEC));
#else
    return fail(errc::not_supported);
#endif

#if defined(OS_TICKS_PER_SEC) ||                                               \
    (defined(MYNEWT_VAL) && defined(MYNEWT_VAL_OS_TICKS_PER_SEC))
    if (freq == 0ULL) {
        return fail(errc::malformed_data);
    }
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>(std::chrono::milliseconds::max().count());
    const std::uint64_t ticks = static_cast<std::uint64_t>(os_time_get());
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

#if defined(SYSCAPE_MYNEWT_HAS_KERNEL_HEADERS)
#undef SYSCAPE_MYNEWT_HAS_KERNEL_HEADERS
#endif

#endif
