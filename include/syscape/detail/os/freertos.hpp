#ifndef SYSCAPE_DETAIL_OS_FREERTOS_HPP
#define SYSCAPE_DETAIL_OS_FREERTOS_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<FreeRTOS.h>)
#include <FreeRTOS.h>
#include <task.h>
#define SYSCAPE_FREERTOS_HAS_KERNEL_HEADERS 1
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

inline result<std::string> parse_version_string(std::string_view content) {
    std::string_view text = content;
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1U);
    }
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n' ||
                             text.back() == ' ' || text.back() == '\t')) {
        text.remove_suffix(1U);
    }
    if (text.empty()) {
        return fail(errc::not_found);
    }
    std::string result_str(text);
    if (!is_valid_utf8(result_str)) {
        return fail(errc::invalid_encoding);
    }
    return result_str;
}

inline result<std::string> product_name() {
    return std::string("FreeRTOS");
}

inline result<std::string> product_version() {
#if defined(tskKERNEL_VERSION_NUMBER)
    return parse_version_string(tskKERNEL_VERSION_NUMBER);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

inline result<std::string> kernel_name() {
    return std::string("FreeRTOS");
}

inline result<std::string> kernel_version() {
#if defined(tskKERNEL_VERSION_NUMBER)
    return parse_version_string(tskKERNEL_VERSION_NUMBER);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> host_name() {
    return fail(errc::not_supported);
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_FREERTOS_HAS_KERNEL_HEADERS)
#if defined(configTICK_RATE_HZ)
    const TickType_t ticks = ::xTaskGetTickCount();
    const auto rate = static_cast<std::uint64_t>(configTICK_RATE_HZ);
    if (rate == 0) {
        return fail(errc::malformed_data);
    }
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>(std::chrono::milliseconds::max().count());
    const auto ticks_u64 = static_cast<std::uint64_t>(ticks);
    if (ticks_u64 > UINT64_MAX / 1000ULL) {
        return fail(errc::value_too_large);
    }
    const auto ms = (ticks_u64 * 1000ULL) / rate;
    if (ms > max_ms) {
        return fail(errc::value_too_large);
    }
    return std::chrono::milliseconds(
        static_cast<std::chrono::milliseconds::rep>(ms));
#elif defined(portTICK_PERIOD_MS)
    const TickType_t ticks = ::xTaskGetTickCount();
    const auto period = static_cast<std::uint64_t>(portTICK_PERIOD_MS);
    if (period == 0) {
        return fail(errc::malformed_data);
    }
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>(std::chrono::milliseconds::max().count());
    const auto ticks_u64 = static_cast<std::uint64_t>(ticks);
    if (ticks_u64 > max_ms / period) {
        return fail(errc::value_too_large);
    }
    const auto ms = ticks_u64 * period;
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

inline result<std::string> boot_identifier() {
    return fail(errc::not_supported);
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#if defined(SYSCAPE_FREERTOS_HAS_KERNEL_HEADERS)
#undef SYSCAPE_FREERTOS_HAS_KERNEL_HEADERS
#endif

#endif
