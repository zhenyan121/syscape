#ifndef SYSCAPE_DETAIL_OS_THREADX_HPP
#define SYSCAPE_DETAIL_OS_THREADX_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<tx_api.h>)
#include <tx_api.h>
#define SYSCAPE_THREADX_HAS_KERNEL_HEADERS 1
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
namespace detail {

inline result<std::string> parse_version_string(const char* content) {
    if (content == nullptr) {
        return fail(errc::not_found);
    }
    const char* text = content;
    const char* version_pos = std::strstr(text, "Version ");
    if (version_pos != nullptr) {
        text = version_pos + 8;
    }
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

} // namespace detail

inline result<std::string> product_name() {
    return std::string("Eclipse ThreadX");
}

inline result<std::string> product_version() {
#if defined(SYSCAPE_THREADX_HAS_KERNEL_HEADERS)
#if defined(THREADX_MAJOR_VERSION) && defined(THREADX_MINOR_VERSION) &&        \
    defined(THREADX_PATCH_VERSION)
    return std::to_string(THREADX_MAJOR_VERSION) + "." +
           std::to_string(THREADX_MINOR_VERSION) + "." +
           std::to_string(THREADX_PATCH_VERSION);
#elif defined(THREADX_MAJOR_VERSION) && defined(THREADX_MINOR_VERSION)
    return std::to_string(THREADX_MAJOR_VERSION) + "." +
           std::to_string(THREADX_MINOR_VERSION);
#else
    return detail::parse_version_string(_tx_version_id);
#endif
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

inline result<std::string> kernel_name() {
    return std::string("ThreadX");
}

inline result<std::string> kernel_version() {
    return product_version();
}

inline result<std::string> host_name() {
    return fail(errc::not_supported);
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_THREADX_HAS_KERNEL_HEADERS)
#if defined(TX_TIMER_TICKS_PER_SECOND)
    const ULONG ticks = ::tx_time_get();
    const auto rate = static_cast<std::uint64_t>(TX_TIMER_TICKS_PER_SECOND);
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

#if defined(SYSCAPE_THREADX_HAS_KERNEL_HEADERS)
#undef SYSCAPE_THREADX_HAS_KERNEL_HEADERS
#endif

#endif
