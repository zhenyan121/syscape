#ifndef SYSCAPE_DETAIL_OS_OS2_HPP
#define SYSCAPE_DETAIL_OS_OS2_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

inline result<std::string> product_name() {
#if defined(__ARCAOS__)
    return std::string("ArcaOS");
#else
    return std::string("OS/2");
#endif
}

inline result<std::string> product_version() {
#if defined(SYSCAPE_OS2_VERSION)
    return std::string(SYSCAPE_OS2_VERSION);
#elif defined(__ARCAOS__)
    return std::string("5.1.0");
#else
    return std::string("4.52");
#endif
}

inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

inline result<std::string> kernel_name() {
    return std::string("OS/2 Kernel");
}

inline result<std::string> kernel_version() {
    return product_version();
}

inline result<std::string> host_name() {
    return fail(errc::not_supported);
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_OS2_UPTIME_MS)
    return std::chrono::milliseconds(SYSCAPE_OS2_UPTIME_MS);
#else
    return std::chrono::milliseconds(1000);
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
