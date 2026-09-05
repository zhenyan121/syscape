#ifndef SYSCAPE_DETAIL_OS_HURD_HPP
#define SYSCAPE_DETAIL_OS_HURD_HPP

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

inline bool read_line(FILE* fp, std::string& out) {
    out.clear();
    char buf[256];
    while (std::fgets(buf, static_cast<int>(sizeof(buf)), fp) != nullptr) {
        out.append(buf);
        if (!out.empty() && out.back() == '\n') {
            return true;
        }
    }
    return !out.empty();
}

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
    return std::string("GNU/Hurd");
}

inline result<std::string> product_version() {
    FILE* fp = std::fopen("/etc/os-release", "r");
    if (fp != nullptr) {
        std::string line;
        std::string version_id;
        while (read_line(fp, line)) {
            if (line.rfind("VERSION_ID=", 0) == 0) {
                std::size_t val = 11;
                while (val < line.size() &&
                       (line[val] == ' ' || line[val] == '"')) {
                    ++val;
                }
                std::size_t end = line.size();
                while (end > val &&
                       (line[end - 1] == '\n' || line[end - 1] == '\r' ||
                        line[end - 1] == ' ' || line[end - 1] == '"')) {
                    --end;
                }
                if (end > val) {
                    version_id = line.substr(val, end - val);
                    break;
                }
            }
        }
        std::fclose(fp);
        if (!version_id.empty()) {
            return version_id;
        }
    }

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
    if (std::strstr(name.version, "Mach") != nullptr ||
        std::strstr(name.version, "mach") != nullptr) {
        return std::string("GNU Mach");
    }
    if (name.sysname[0] != '\0') {
        return std::string(name.sysname);
    }
    return std::string("GNU Mach");
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
    if (name.version[0] != '\0') {
        return std::string(name.version);
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
    {
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
    }

    FILE* fp = std::fopen("/proc/uptime", "r");
    if (fp != nullptr) {
        double up_secs = 0.0;
        const int count = std::fscanf(fp, "%lf", &up_secs);
        const int read_err = std::ferror(fp) ? errno : 0;
        std::fclose(fp);
        if (read_err != 0) {
            if (read_err == EACCES || read_err == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(read_err, std::generic_category()));
        }
        if (count == 1 && std::isfinite(up_secs) && up_secs >= 0.0) {
            constexpr double max_seconds =
                static_cast<double>((std::chrono::seconds::max)().count());
            if (up_secs <= max_seconds) {
                const auto now = std::chrono::system_clock::now();
                const auto dur = std::chrono::duration_cast<
                    std::chrono::system_clock::duration>(
                    std::chrono::duration<double>(up_secs));
                if (dur <= now.time_since_epoch()) {
                    return now - dur;
                }
            }
            return fail(errc::value_too_large);
        }
        return fail(errc::malformed_data);
    } else {
        const int err = errno;
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err != ENOENT) {
            return fail(std::error_code(err, std::generic_category()));
        }
    }

    return fail(errc::not_found);
}

inline result<std::chrono::milliseconds> uptime() {
    FILE* fp = std::fopen("/proc/uptime", "r");
    if (fp != nullptr) {
        double up_secs = 0.0;
        const int count = std::fscanf(fp, "%lf", &up_secs);
        const int read_err = std::ferror(fp) ? errno : 0;
        std::fclose(fp);
        if (read_err != 0) {
            if (read_err == EACCES || read_err == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(read_err, std::generic_category()));
        }
        if (count == 1 && std::isfinite(up_secs) && up_secs >= 0.0) {
            constexpr double max_seconds = static_cast<double>(
                (std::chrono::milliseconds::max)().count() / 1000);
            if (up_secs <= max_seconds) {
                return std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::duration<double>(up_secs));
            }
            return fail(errc::value_too_large);
        }
        return fail(errc::malformed_data);
    } else {
        const int err = errno;
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err != ENOENT) {
            return fail(std::error_code(err, std::generic_category()));
        }
    }
    return fail(errc::not_supported);
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
