#ifndef SYSCAPE_DETAIL_OS_OPENVMS_HPP
#define SYSCAPE_DETAIL_OS_OPENVMS_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

/// Returns the OpenVMS operating system product name.
inline result<std::string> product_name() {
    return std::string("OpenVMS");
}

/// Returns the OpenVMS product version if supplied by the
/// SYSCAPE_OPENVMS_VERSION override macro; otherwise returns not_supported to
/// avoid substituting an unverified version.
inline result<std::string> product_version() {
#if defined(SYSCAPE_OPENVMS_VERSION)
    return std::string(SYSCAPE_OPENVMS_VERSION);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

/// Returns the OpenVMS kernel name determined by compile-time architecture
/// target macros (such as "OpenVMS AXP" on Alpha, "OpenVMS I64" on Itanium,
/// "OpenVMS x86_64" on x86-64, or "OpenVMS VAX" on VAX).
inline result<std::string> kernel_name() {
#if defined(__alpha) || defined(__ALPHA)
    return std::string("OpenVMS AXP");
#elif defined(__ia64) || defined(__IA64) || defined(__ia64__)
    return std::string("OpenVMS I64");
#elif defined(__x86_64) || defined(__x86_64__) || defined(_M_X64)
    return std::string("OpenVMS x86_64");
#elif defined(__vax) || defined(__VAX)
    return std::string("OpenVMS VAX");
#else
    return std::string("OpenVMS Kernel");
#endif
}

/// Returns the kernel version if supplied by the
/// SYSCAPE_OPENVMS_KERNEL_VERSION override macro, or defaults to the product
/// version.
inline result<std::string> kernel_version() {
#if defined(SYSCAPE_OPENVMS_KERNEL_VERSION)
    return std::string(SYSCAPE_OPENVMS_KERNEL_VERSION);
#else
    return product_version();
#endif
}

inline result<std::string> host_name() {
#if defined(SYSCAPE_OPENVMS_HOST_NAME)
    return std::string(SYSCAPE_OPENVMS_HOST_NAME);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_OPENVMS_UPTIME_MS)
    return std::chrono::milliseconds(SYSCAPE_OPENVMS_UPTIME_MS);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::chrono::system_clock::time_point> boot_time() {
    const auto up = uptime();
    if (!up) {
        return fail(up.error());
    }
    const auto now = std::chrono::system_clock::now();
    const auto duration =
        std::chrono::duration_cast<std::chrono::system_clock::duration>(*up);
    if (duration > now.time_since_epoch()) {
        return fail(errc::malformed_data);
    }
    return now - duration;
}

inline result<std::string> boot_identifier() {
    return fail(errc::not_supported);
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
