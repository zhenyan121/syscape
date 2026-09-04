#ifndef SYSCAPE_DETAIL_OS_AIX_HPP
#define SYSCAPE_DETAIL_OS_AIX_HPP

#if !defined(_BSD)
#define _BSD 44
#endif

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <system_error>
#include <unistd.h>
#include <utmpx.h>
#include <vector>

#include <sys/utsname.h>

#include <syscape/detail/posix/utmpx.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

inline result<std::string> product_name() {
    return std::string("AIX");
}

inline result<std::string> product_version() {
    struct ::utsname name {};
    if (::uname(&name) == 0 && name.release[0] != '\0') {
        std::string ver = name.version;
        ver += ".";
        ver += name.release;
        return ver;
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
    return std::string("AIX");
}

inline result<std::string> kernel_version() {
    struct ::utsname name {};
    if (::uname(&name) == 0 && name.release[0] != '\0') {
        std::string ver = name.version;
        ver += ".";
        ver += name.release;
        return ver;
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
                return std::string(buffer.data(), end);
            }
            buffer.resize(buffer.size() * 2U);
            continue;
        }
        if (errno != ENAMETOOLONG && errno != EINVAL) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        buffer.resize(buffer.size() * 2U);
    }
    return fail(errc::value_too_large);
}

inline result<std::string> boot_identifier() {
    return fail(errc::not_supported);
}

inline result<std::chrono::system_clock::time_point> boot_time() {
    std::lock_guard<std::mutex> lock(posix_utmpx::mutex());
    ::setutxent();
    struct utmpx_cleanup {
        ~utmpx_cleanup() {
            ::endutxent();
        }
    } cleanup;

    while (const ::utmpx* entry = ::getutxent()) {
        if (entry->ut_type == BOOT_TIME) {
            if (entry->ut_tv.tv_sec > 0) {
                const auto sec = std::chrono::seconds(entry->ut_tv.tv_sec);
                const auto usec =
                    std::chrono::microseconds(entry->ut_tv.tv_usec);
                return std::chrono::system_clock::time_point(
                    std::chrono::duration_cast<
                        std::chrono::system_clock::duration>(sec + usec));
            }
        }
    }
    return fail(errc::not_found);
}

inline result<std::chrono::milliseconds> uptime() {
    const auto boot = boot_time();
    if (!boot) {
        return fail(boot.error());
    }
    const auto now = std::chrono::system_clock::now();
    if (now < *boot) {
        return fail(errc::malformed_data);
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - *boot);
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
