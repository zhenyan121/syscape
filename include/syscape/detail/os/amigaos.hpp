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

inline result<std::string> product_name() {
#if defined(__MORPHOS__) || defined(__morphos__)
    return std::string("MorphOS");
#elif defined(__amigaos4__)
    return std::string("AmigaOS 4");
#else
    return std::string("AmigaOS");
#endif
}

inline result<std::string> product_version() {
#if defined(SYSCAPE_AMIGAOS_VERSION)
    return std::string(SYSCAPE_AMIGAOS_VERSION);
#elif defined(__MORPHOS__) || defined(__morphos__)
    return std::string("3.18");
#elif defined(__amigaos4__)
    return std::string("4.1");
#else
    return std::string("3.1");
#endif
}

inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

inline result<std::string> kernel_name() {
#if defined(__MORPHOS__) || defined(__morphos__)
    return std::string("Quark");
#else
    return std::string("Exec");
#endif
}

inline result<std::string> kernel_version() {
    return product_version();
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
