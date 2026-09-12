#ifndef SYSCAPE_DETAIL_OS_TKERNEL_HPP
#define SYSCAPE_DETAIL_OS_TKERNEL_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<tk/tkernel.h>)
#include <tk/tkernel.h>
#define SYSCAPE_TKERNEL_HAS_KERNEL_HEADERS 1
#elif __has_include(<tkernel.h>)
#include <tkernel.h>
#define SYSCAPE_TKERNEL_HAS_KERNEL_HEADERS 1
#elif __has_include(<t-kernel.h>)
#include <t-kernel.h>
#define SYSCAPE_TKERNEL_HAS_KERNEL_HEADERS 1
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

template <typename T>
inline auto extract_systim_ms(const T& t, int)
    -> decltype((void)t.hi, (void)t.lo, std::uint64_t {}) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(t.hi))
            << 32) |
           static_cast<std::uint64_t>(t.lo);
}

template <typename T>
inline std::uint64_t extract_systim_ms(const T& t, ...) {
    return static_cast<std::uint64_t>(t);
}

inline result<std::string> product_name() {
#if defined(_uTKERNEL_) || defined(_UTKERNEL_) || defined(__uTKERNEL__) ||     \
    defined(__UTKERNEL__) || defined(UTK_VERSION)
    return std::string("TRON \xC2\xB5T-Kernel");
#else
    return std::string("T-Kernel");
#endif
}

namespace detail {

inline result<std::string> format_tk_version(std::uint32_t ver) {
    if (ver == 0) {
        return fail(errc::not_found);
    }
    if (ver >= 10000U) {
        const std::uint32_t major = ver / 10000;
        const std::uint32_t minor = (ver / 100) % 100;
        const std::uint32_t patch = ver % 100;
        return std::to_string(major) + "." + std::to_string(minor) + "." +
               std::to_string(patch);
    }
    if (ver >= 1000U) {
        const std::uint32_t major = ver / 1000;
        const std::uint32_t minor = (ver / 10) % 100;
        const std::uint32_t patch = ver % 10;
        return std::to_string(major) + "." + std::to_string(minor) + "." +
               std::to_string(patch);
    }
    const std::uint32_t major = ver / 100;
    const std::uint32_t minor = (ver / 10) % 10;
    const std::uint32_t patch = ver % 10;
    return std::to_string(major) + "." + std::to_string(minor) + "." +
           std::to_string(patch);
}

} // namespace detail

inline result<std::string> product_version() {
#if defined(TK_MAJOR_VERSION) && defined(TK_MINOR_VERSION) &&                  \
    defined(TK_PATCH_VERSION)
    return std::to_string(TK_MAJOR_VERSION) + "." +
           std::to_string(TK_MINOR_VERSION) + "." +
           std::to_string(TK_PATCH_VERSION);
#elif defined(TK_MAJOR_VERSION) && defined(TK_MINOR_VERSION)
    return std::to_string(TK_MAJOR_VERSION) + "." +
           std::to_string(TK_MINOR_VERSION) + ".0";
#elif defined(UTK_MAJOR_VERSION) && defined(UTK_MINOR_VERSION) &&              \
    defined(UTK_PATCH_VERSION)
    return std::to_string(UTK_MAJOR_VERSION) + "." +
           std::to_string(UTK_MINOR_VERSION) + "." +
           std::to_string(UTK_PATCH_VERSION);
#elif defined(UTK_MAJOR_VERSION) && defined(UTK_MINOR_VERSION)
    return std::to_string(UTK_MAJOR_VERSION) + "." +
           std::to_string(UTK_MINOR_VERSION) + ".0";
#elif defined(TK_VERSION)
    return detail::format_tk_version(static_cast<std::uint32_t>(TK_VERSION));
#elif defined(SYSCAPE_TKERNEL_HAS_KERNEL_HEADERS)
    T_RVER rver {};
    if (::tk_ref_ver(&rver) == 0) {
        const std::uint16_t ver = static_cast<std::uint16_t>(rver.prver);
        if (ver > 0) {
            const std::uint32_t major = (ver >> 8) & 0xFF;
            const std::uint32_t minor = ver & 0xFF;
            const std::uint32_t patch =
                static_cast<std::uint32_t>(rver.prno[0] & 0xFF);
            return std::to_string(major) + "." + std::to_string(minor) + "." +
                   std::to_string(patch);
        }
    }
    return fail(errc::temporarily_unavailable);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

inline result<std::string> kernel_name() {
    return std::string("T-Kernel");
}

inline result<std::string> kernel_version() {
    return product_version();
}

inline result<std::string> host_name() {
    return fail(errc::not_supported);
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_TKERNEL_HAS_KERNEL_HEADERS)
    SYSTIM tim {};
    if (::tk_get_tim(&tim) != 0) {
        return fail(errc::temporarily_unavailable);
    }
    const std::uint64_t ms = extract_systim_ms(tim, 0);
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>(std::chrono::milliseconds::max().count());
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

#if defined(SYSCAPE_TKERNEL_HAS_KERNEL_HEADERS)
#undef SYSCAPE_TKERNEL_HAS_KERNEL_HEADERS
#endif

#endif
