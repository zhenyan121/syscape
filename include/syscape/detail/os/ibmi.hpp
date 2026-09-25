#ifndef SYSCAPE_DETAIL_OS_IBMI_HPP
#define SYSCAPE_DETAIL_OS_IBMI_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <string>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

/// Returns the IBM i operating system product name.
inline result<std::string> product_name() {
    return std::string("IBM i");
}

/// Returns the IBM i product version if supplied by the SYSCAPE_IBMI_VERSION
/// override macro or decoded from __OS400_TGTVRM__; otherwise returns
/// not_supported to avoid substituting an unverified version.
inline result<std::string> product_version() {
#if defined(SYSCAPE_IBMI_VERSION)
    return std::string(SYSCAPE_IBMI_VERSION);
#elif defined(__OS400_TGTVRM__)
#if __OS400_TGTVRM__ > 0
    // Decode 3-digit VRM integer representation where hundreds digit is
    // version, tens digit is release, and units digit is modification (e.g.,
    // 740 -> V7R4M0).
    constexpr unsigned int vrm = static_cast<unsigned int>(__OS400_TGTVRM__);
    const unsigned int v = vrm / 100U;
    const unsigned int r = (vrm / 10U) % 10U;
    const unsigned int m = vrm % 10U;
    return std::string("V") + std::to_string(v) + "R" + std::to_string(r) +
           "M" + std::to_string(m);
#else
    return fail(errc::malformed_data);
#endif
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

/// Returns the IBM i kernel subsystem name.
inline result<std::string> kernel_name() {
#if defined(_PASE) || defined(__PASE__)
    return std::string("PASE for i");
#else
    return std::string("OS/400");
#endif
}

/// Returns the kernel version if supplied by the
/// SYSCAPE_IBMI_KERNEL_VERSION override macro; otherwise returns
/// not_supported to prevent substituting an unverified version.
inline result<std::string> kernel_version() {
#if defined(SYSCAPE_IBMI_KERNEL_VERSION)
    return std::string(SYSCAPE_IBMI_KERNEL_VERSION);
#elif defined(__OS400_TGTVRM__)
#if __OS400_TGTVRM__ > 0
    // Decode 3-digit VRM integer representation where hundreds digit is
    // version, tens digit is release, and units digit is modification (e.g.,
    // 740 -> V7R4M0).
    constexpr unsigned int vrm = static_cast<unsigned int>(__OS400_TGTVRM__);
    const unsigned int v = vrm / 100U;
    const unsigned int r = (vrm / 10U) % 10U;
    const unsigned int m = vrm % 10U;
    return std::string("V") + std::to_string(v) + "R" + std::to_string(r) +
           "M" + std::to_string(m);
#else
    return fail(errc::malformed_data);
#endif
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> host_name() {
#if defined(SYSCAPE_IBMI_HOST_NAME)
    return std::string(SYSCAPE_IBMI_HOST_NAME);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_IBMI_UPTIME_MS)
    return std::chrono::milliseconds(SYSCAPE_IBMI_UPTIME_MS);
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
