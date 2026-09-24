#ifndef SYSCAPE_DETAIL_OS_RISCOS_HPP
#define SYSCAPE_DETAIL_OS_RISCOS_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

/// Returns the RISC OS product name determined by compile-time target
/// detection.
inline result<std::string> product_name() {
    return std::string("RISC OS");
}

/// Returns the RISC OS product version if supplied by the
/// SYSCAPE_RISCOS_VERSION override macro; otherwise returns not_supported to
/// avoid substituting an unverified version.
inline result<std::string> product_version() {
#if defined(SYSCAPE_RISCOS_VERSION)
    return std::string(SYSCAPE_RISCOS_VERSION);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

/// Returns the kernel name for RISC OS.
inline result<std::string> kernel_name() {
    return std::string("RISC OS Kernel");
}

/// Returns the kernel version if supplied by the
/// SYSCAPE_RISCOS_KERNEL_VERSION override macro; otherwise returns
/// not_supported.
inline result<std::string> kernel_version() {
#if defined(SYSCAPE_RISCOS_KERNEL_VERSION)
    return std::string(SYSCAPE_RISCOS_KERNEL_VERSION);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> host_name() {
    return fail(errc::not_supported);
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_RISCOS_UPTIME_MS)
    return std::chrono::milliseconds(SYSCAPE_RISCOS_UPTIME_MS);
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
