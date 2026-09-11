#ifndef SYSCAPE_DETAIL_OS_UCOS_HPP
#define SYSCAPE_DETAIL_OS_UCOS_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<os.h>)
#include <os.h>
#define SYSCAPE_UCOS_HAS_KERNEL_HEADERS 1
#elif __has_include(<ucos_ii.h>)
#include <ucos_ii.h>
#define SYSCAPE_UCOS_HAS_KERNEL_HEADERS 1
#elif __has_include(<ucos_iii.h>)
#include <ucos_iii.h>
#define SYSCAPE_UCOS_HAS_KERNEL_HEADERS 1
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
#if defined(OS_VERSION) && (OS_VERSION >= 30000U)
    return std::string("Micrium uC/OS-III");
#elif defined(UCOS_III)
    return std::string("Micrium uC/OS-III");
#elif defined(OS_VERSION) && (OS_VERSION < 30000U)
    return std::string("Micrium uC/OS-II");
#elif defined(UCOS_II)
    return std::string("Micrium uC/OS-II");
#else
    return std::string("Micrium uC/OS");
#endif
}

inline result<std::string> product_version() {
#if defined(SYSCAPE_UCOS_HAS_KERNEL_HEADERS)
    std::uint32_t ver = 0;
#if defined(OS_VERSION)
    ver = static_cast<std::uint32_t>(OS_VERSION);
#endif
    if (ver == 0) {
        return fail(errc::not_found);
    }
    if (ver >= 10000U) {
        const std::uint32_t major = ver / 10000;
        const std::uint32_t minor = (ver / 100) % 100;
        const std::uint32_t patch = ver % 100;
        return std::to_string(major) + "." + std::to_string(minor) + "." +
               std::to_string(patch);
    } else if (ver >= 100U) {
        const std::uint32_t major = ver / 100;
        const std::uint32_t minor = ver % 100;
        return std::to_string(major) + "." + std::to_string(minor) + ".0";
    }
    return std::to_string(ver);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

inline result<std::string> kernel_name() {
#if defined(OS_VERSION) && (OS_VERSION >= 30000U)
    return std::string("uC/OS-III");
#elif defined(UCOS_III)
    return std::string("uC/OS-III");
#elif defined(OS_VERSION) && (OS_VERSION < 30000U)
    return std::string("uC/OS-II");
#elif defined(UCOS_II)
    return std::string("uC/OS-II");
#else
    return std::string("uC/OS");
#endif
}

inline result<std::string> kernel_version() {
    return product_version();
}

inline result<std::string> host_name() {
    return fail(errc::not_supported);
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_UCOS_HAS_KERNEL_HEADERS)
#if defined(OS_CFG_TICK_RATE_HZ)
    const auto rate = static_cast<std::uint64_t>(OS_CFG_TICK_RATE_HZ);
#elif defined(OS_TICKS_PER_SEC)
    const auto rate = static_cast<std::uint64_t>(OS_TICKS_PER_SEC);
#else
    const auto rate = static_cast<std::uint64_t>(1000U);
#endif
    if (rate == 0) {
        return fail(errc::malformed_data);
    }

#if defined(OS_CFG_TICK_RATE_HZ) || defined(UCOS_III) ||                       \
    (defined(OS_VERSION) && OS_VERSION >= 30000U)
    OS_ERR err = 0;
    const auto ticks = ::OSTimeGet(&err);
#else
    const auto ticks = ::OSTimeGet();
#endif

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

#if defined(SYSCAPE_UCOS_HAS_KERNEL_HEADERS)
#undef SYSCAPE_UCOS_HAS_KERNEL_HEADERS
#endif

#endif
