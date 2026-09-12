#ifndef SYSCAPE_DETAIL_OS_INTEGRITY_HPP
#define SYSCAPE_DETAIL_OS_INTEGRITY_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<INTEGRITY.h>)
#include <INTEGRITY.h>
#define SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS 1
#elif __has_include(<integrity.h>)
#include <integrity.h>
#define SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS 1
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
    return std::string("Green Hills INTEGRITY");
}

inline result<std::string> product_version() {
#if defined(INTEGRITY_MAJOR_VERSION) && defined(INTEGRITY_MINOR_VERSION) &&    \
    defined(INTEGRITY_PATCH_VERSION)
    return std::to_string(INTEGRITY_MAJOR_VERSION) + "." +
           std::to_string(INTEGRITY_MINOR_VERSION) + "." +
           std::to_string(INTEGRITY_PATCH_VERSION);
#elif defined(INTEGRITY_MAJOR_VERSION) && defined(INTEGRITY_MINOR_VERSION)
    return std::to_string(INTEGRITY_MAJOR_VERSION) + "." +
           std::to_string(INTEGRITY_MINOR_VERSION) + ".0";
#elif defined(__INTEGRITY_MAJOR_VERSION__) &&                                  \
    defined(__INTEGRITY_MINOR_VERSION__) &&                                    \
    defined(__INTEGRITY_PATCH_VERSION__)
    return std::to_string(__INTEGRITY_MAJOR_VERSION__) + "." +
           std::to_string(__INTEGRITY_MINOR_VERSION__) + "." +
           std::to_string(__INTEGRITY_PATCH_VERSION__);
#elif defined(__INTEGRITY_MAJOR_VERSION__) &&                                  \
    defined(__INTEGRITY_MINOR_VERSION__)
    return std::to_string(__INTEGRITY_MAJOR_VERSION__) + "." +
           std::to_string(__INTEGRITY_MINOR_VERSION__) + ".0";
#elif defined(INTEGRITY_VERSION)
    const auto ver = static_cast<std::uint32_t>(INTEGRITY_VERSION);
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
    const std::uint32_t major = ver / 100;
    const std::uint32_t minor = (ver / 10) % 10;
    const std::uint32_t patch = ver % 10;
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
    return std::string("INTEGRITY");
}

inline result<std::string> kernel_version() {
    return product_version();
}

inline result<std::string> host_name() {
    return fail(errc::not_supported);
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS)
    Time t = 0;
    if (::GetTime(&t) != 0) {
        return fail(errc::temporarily_unavailable);
    }
    constexpr std::uint64_t max_ms =
        static_cast<std::uint64_t>(std::chrono::milliseconds::max().count());
    // GetTime returns nanoseconds; convert to milliseconds.
    const auto t_u64 = static_cast<std::uint64_t>(t);
    const auto ms = t_u64 / 1000000ULL;
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

#if defined(SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS)
#undef SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS
#endif

#endif
