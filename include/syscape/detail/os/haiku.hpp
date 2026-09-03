#ifndef SYSCAPE_DETAIL_OS_HAIKU_HPP
#define SYSCAPE_DETAIL_OS_HAIKU_HPP

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <sys/utsname.h>

#if defined(__has_include)
#if __has_include(<OS.h>)
#include <OS.h>
#define SYSCAPE_HAS_HAIKU_OS_H 1
#endif
#endif

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

inline result<std::string> product_name() {
    return std::string("Haiku");
}

inline result<std::string> product_version() {
    struct ::utsname name {};
    if (::uname(&name) == 0 && name.release[0] != '\0') {
        return std::string(name.release);
    }
    return fail(errc::not_found);
}

inline result<std::string> build_identifier() {
    struct ::utsname name {};
    if (::uname(&name) == 0 && name.version[0] != '\0') {
        return std::string(name.version);
    }
    return fail(errc::not_found);
}

inline result<std::string> kernel_name() {
    struct ::utsname name {};
    if (::uname(&name) == 0 && name.sysname[0] != '\0') {
        return std::string(name.sysname);
    }
    return std::string("Haiku");
}

inline result<std::string> kernel_version() {
    struct ::utsname name {};
    if (::uname(&name) == 0 && name.version[0] != '\0') {
        return std::string(name.version);
    }
    return fail(errc::not_found);
}

inline result<std::string> host_name() {
    std::vector<char> buffer(256U);
    constexpr std::size_t maximum_size = 1024U * 1024U;
    while (buffer.size() <= maximum_size) {
        errno = 0;
        if (::gethostname(buffer.data(), buffer.size()) == 0) {
            std::size_t end = 0U;
            while (end < buffer.size() && buffer[end] != '\0') {
                ++end;
            }
            if (end < buffer.size()) {
                if (end > 0) {
                    return std::string(buffer.data(), end);
                }
                break;
            }
            buffer.resize(buffer.size() * 2U);
            continue;
        }
        if (errno != ENAMETOOLONG && errno != EINVAL) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        buffer.resize(buffer.size() * 2U);
    }
    return fail(errc::not_found);
}

inline result<std::string> boot_identifier() {
    return fail(errc::not_supported);
}

inline result<std::chrono::system_clock::time_point> boot_time() {
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::system_info info {};
    if (::get_system_info(&info) == B_OK && info.boot_time > 0) {
        const auto us = std::chrono::microseconds(info.boot_time);
        return std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                us));
    }
    return fail(errc::io_error);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    const bigtime_t us = ::system_time();
    if (us >= 0) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::microseconds(us));
    }
    return fail(errc::io_error);
#else
    return fail(errc::not_supported);
#endif
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
