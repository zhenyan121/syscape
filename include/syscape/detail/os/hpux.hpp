#ifndef SYSCAPE_DETAIL_OS_HPUX_HPP
#define SYSCAPE_DETAIL_OS_HPUX_HPP

#include <syscape/detail/config.hpp>

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

inline result<std::chrono::system_clock::time_point>
timeval_to_time_point(std::int64_t seconds, std::int64_t microseconds) {
    if (seconds < 0 || microseconds < 0 || microseconds >= 1000000) {
        return fail(errc::malformed_data);
    }
    using clock = std::chrono::system_clock;
    const std::int64_t maximum_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(clock::duration::max())
            .count();
    if (seconds > maximum_seconds) {
        return fail(errc::value_too_large);
    }
    const clock::duration whole = std::chrono::duration_cast<clock::duration>(
        std::chrono::seconds(seconds));
    const clock::duration fraction =
        std::chrono::duration_cast<clock::duration>(
            std::chrono::microseconds(microseconds));
    if (fraction > clock::duration::max() - whole) {
        return fail(errc::value_too_large);
    }
    return clock::time_point(whole + fraction);
}

inline result<std::string> product_name() {
    return std::string("HP-UX");
}

inline result<std::string> product_version() {
    struct ::utsname name {};
    errno = 0;
    if (::uname(&name) != 0) {
        const int saved_errno = errno;
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::io_error);
    }
    if (name.release[0] != '\0') {
        std::string ver = name.release;
        if (ver.size() > 2U && ver[0] == 'B' && ver[1] == '.') {
            ver = ver.substr(2U);
        }
        return ver;
    }
    return fail(errc::not_found);
}

inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

inline result<std::string> kernel_name() {
    struct ::utsname name {};
    errno = 0;
    if (::uname(&name) != 0) {
        const int saved_errno = errno;
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::io_error);
    }
    if (name.sysname[0] != '\0') {
        return std::string(name.sysname);
    }
    return fail(errc::not_found);
}

inline result<std::string> kernel_version() {
    struct ::utsname name {};
    errno = 0;
    if (::uname(&name) != 0) {
        const int saved_errno = errno;
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::io_error);
    }
    if (name.release[0] != '\0') {
        return std::string(name.release);
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
                return timeval_to_time_point(entry->ut_tv.tv_sec,
                                             entry->ut_tv.tv_usec);
            }
        }
    }
    return fail(errc::not_found);
}

inline result<std::chrono::milliseconds> uptime() {
    return fail(errc::not_supported);
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
