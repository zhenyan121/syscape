#ifndef SYSCAPE_DETAIL_OS_AMIGAOS_HPP
#define SYSCAPE_DETAIL_OS_AMIGAOS_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

/// Returns the AmigaOS family product name determined by compile-time target
/// macros (such as "MorphOS" under __MORPHOS__, "AmigaOS 4" under __amigaos4__,
/// or "AmigaOS" otherwise).
inline result<std::string> product_name() {
#if defined(__MORPHOS__) || defined(__morphos__)
    return std::string("MorphOS");
#elif defined(__amigaos4__)
    return std::string("AmigaOS 4");
#else
    return std::string("AmigaOS");
#endif
}

/// Returns the AmigaOS product version if supplied by the
/// SYSCAPE_AMIGAOS_VERSION override macro; otherwise returns not_supported to
/// avoid substituting an unverified version.
inline result<std::string> product_version() {
#if defined(SYSCAPE_AMIGAOS_VERSION)
    return std::string(SYSCAPE_AMIGAOS_VERSION);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

/// Returns the kernel name determined by compile-time target macros ("Quark"
/// for MorphOS, "Exec" for AmigaOS).
inline result<std::string> kernel_name() {
#if defined(__MORPHOS__) || defined(__morphos__)
    return std::string("Quark");
#else
    return std::string("Exec");
#endif
}

/// Returns the kernel version if supplied by the
/// SYSCAPE_AMIGAOS_KERNEL_VERSION override macro; otherwise returns
/// not_supported.
inline result<std::string> kernel_version() {
#if defined(SYSCAPE_AMIGAOS_KERNEL_VERSION)
    return std::string(SYSCAPE_AMIGAOS_KERNEL_VERSION);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> host_name() {
    return fail(errc::not_supported);
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_AMIGAOS_UPTIME_MS)
    return std::chrono::milliseconds(SYSCAPE_AMIGAOS_UPTIME_MS);
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

#endif
