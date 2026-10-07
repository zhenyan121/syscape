#ifndef SYSCAPE_DETAIL_OS_CMSIS_RTOS_HPP
#define SYSCAPE_DETAIL_OS_CMSIS_RTOS_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>

#if defined(__has_include)
#if defined(__CMSIS_RTOS) && !defined(__CMSIS_RTOS2) && __has_include(<cmsis_os.h>)
#include <cmsis_os.h>
#define SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS 1
#elif __has_include(<cmsis_os2.h>)
#include <cmsis_os2.h>
#define SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS 1
#elif __has_include(<cmsis_os.h>)
#include <cmsis_os.h>
#define SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS 1
#endif
#endif

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

inline result<std::string> product_name() {
#if defined(osCMSIS_RTX5) || defined(osCMSIS_RTX) || defined(SYSCAPE_TARGET_RTX)
    return std::string("Keil RTX");
#else
    return std::string("CMSIS-RTOS");
#endif
}

inline result<std::string> kernel_name() {
#if defined(osCMSIS_RTX5)
    return std::string("RTX5");
#elif defined(osCMSIS_RTX)
    return std::string("RTX");
#else
    return std::string("CMSIS-RTOS");
#endif
}

inline result<std::string> product_version() {
#if defined(SYSCAPE_CMSIS_RTOS_VERSION_STRING)
    return std::string(SYSCAPE_CMSIS_RTOS_VERSION_STRING);
#elif defined(SYSCAPE_RTX_VERSION_STRING)
    return std::string(SYSCAPE_RTX_VERSION_STRING);
#elif defined(SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS)
#if defined(osCMSIS) && (osCMSIS >= 0x20000U)
    osVersion_t ver {};
    char id_buf[64] = {0};
    if (osKernelGetInfo(&ver, id_buf, sizeof(id_buf)) == osOK) {
        if (ver.kernel != 0U) {
            const std::uint32_t major = ver.kernel / 10000000U;
            const std::uint32_t minor = (ver.kernel / 10000U) % 1000U;
            const std::uint32_t patch = ver.kernel % 10000U;
            return std::to_string(major) + "." + std::to_string(minor) + "." +
                   std::to_string(patch);
        }
        if (id_buf[0] != '\0') {
            return std::string(id_buf);
        }
    }
#endif
#if defined(osRtxVersionKernel)
    const std::uint32_t rtx_ver =
        static_cast<std::uint32_t>(osRtxVersionKernel);
    const std::uint32_t major = rtx_ver / 10000000U;
    const std::uint32_t minor = (rtx_ver / 10000U) % 1000U;
    const std::uint32_t patch = rtx_ver % 10000U;
    return std::to_string(major) + "." + std::to_string(minor) + "." +
           std::to_string(patch);
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

// Computes monotonic uptime from CMSIS-RTOS2 osKernelGetTickCount() and
// osKernelGetTickFreq() or CMSIS-RTOS v1 osKernelSysTick() and
// osKernelSysTickFrequency.
inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS)
#if defined(osCMSIS) && (osCMSIS >= 0x20000U)
    const auto freq = static_cast<std::uint64_t>(osKernelGetTickFreq());
    if (freq == 0ULL) {
        return fail(errc::malformed_data);
    }
    const auto ticks = static_cast<std::uint64_t>(osKernelGetTickCount());
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>((std::chrono::milliseconds::max)().count());
    if (ticks > UINT64_MAX / 1000ULL) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t ms = (ticks * 1000ULL) / freq;
    if (ms > max_ms) {
        return fail(errc::value_too_large);
    }
    return std::chrono::milliseconds(
        static_cast<std::chrono::milliseconds::rep>(ms));
#elif defined(osKernelSysTickFrequency)
    const auto freq = static_cast<std::uint64_t>(osKernelSysTickFrequency);
    if (freq == 0ULL) {
        return fail(errc::malformed_data);
    }
    const auto ticks = static_cast<std::uint64_t>(osKernelSysTick());
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>((std::chrono::milliseconds::max)().count());
    if (ticks > UINT64_MAX / 1000ULL) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t ms = (ticks * 1000ULL) / freq;
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

#if defined(SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS)
#undef SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS
#endif

#endif // SYSCAPE_DETAIL_OS_CMSIS_RTOS_HPP
