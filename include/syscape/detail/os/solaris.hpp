#ifndef SYSCAPE_DETAIL_OS_SOLARIS_HPP
#define SYSCAPE_DETAIL_OS_SOLARIS_HPP

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <sys/systeminfo.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <system_error>
#include <unistd.h>
#include <utmpx.h>
#include <utility>
#include <vector>

#include <syscape/detail/posix/utmpx.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

inline result<std::string> sysinfo_query(int command) {
    std::size_t size = 256U;
    for (int attempts = 0; attempts < 4; ++attempts) {
        std::string buffer(size, '\0');
        const long len =
            ::sysinfo(command, &buffer[0], static_cast<long>(size));
        if (len <= 0) {
            if (errno == EINVAL) {
                return fail(errc::not_supported);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (static_cast<std::size_t>(len) <= size) {
            while (!buffer.empty() &&
                   (buffer.back() == '\0' || buffer.back() == '\n' ||
                    buffer.back() == '\r')) {
                buffer.pop_back();
            }
            if (buffer.empty()) {
                return fail(errc::not_found);
            }
            return buffer;
        }
        size = static_cast<std::size_t>(len) + 16U;
    }
    return fail(errc::malformed_data);
}

inline result<std::string> product_name() {
    std::ifstream release_file("/etc/release");
    if (release_file.is_open()) {
        std::string line;
        if (std::getline(release_file, line)) {
            const std::size_t start = line.find_first_not_of(" \t\r\n");
            if (start != std::string::npos) {
                const std::size_t end = line.find_last_not_of(" \t\r\n");
                line = line.substr(start, end - start + 1U);
                if (!line.empty()) {
                    return line;
                }
            }
        }
    }

    struct ::utsname name {};
    if (::uname(&name) == 0 && name.sysname[0] != '\0') {
        return std::string(name.sysname);
    }
    return sysinfo_query(SI_SYSNAME);
}

inline result<std::string> product_version() {
    struct ::utsname name {};
    if (::uname(&name) == 0 && name.release[0] != '\0') {
        return std::string(name.release);
    }
    return sysinfo_query(SI_RELEASE);
}

inline result<std::string> build_identifier() {
    struct ::utsname name {};
    if (::uname(&name) == 0 && name.version[0] != '\0') {
        return std::string(name.version);
    }
    return sysinfo_query(SI_VERSION);
}

inline result<std::string> kernel_name() {
    struct ::utsname name {};
    if (::uname(&name) == 0 && name.sysname[0] != '\0') {
        return std::string(name.sysname);
    }
    return sysinfo_query(SI_SYSNAME);
}

inline result<std::string> kernel_version() {
    struct ::utsname name {};
    if (::uname(&name) == 0 && name.release[0] != '\0') {
        std::string ver(name.release);
        if (name.version[0] != '\0') {
            ver += "-";
            ver += name.version;
        }
        return ver;
    }
    return sysinfo_query(SI_RELEASE);
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
            break;
        }
        buffer.resize(buffer.size() * 2U);
    }
    return sysinfo_query(SI_HOSTNAME);
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
#if defined(CLOCK_BOOTTIME)
    struct timespec ts {};
    if (::clock_gettime(CLOCK_BOOTTIME, &ts) == 0) {
        if (ts.tv_sec < 0 || ts.tv_nsec < 0) {
            return fail(errc::malformed_data);
        }
        const auto sec = std::chrono::seconds(ts.tv_sec);
        const auto nsec = std::chrono::nanoseconds(ts.tv_nsec);
        return std::chrono::duration_cast<std::chrono::milliseconds>(sec +
                                                                     nsec);
    }
    if (errno != 0 && errno != EINVAL && errno != ENOSYS) {
        return fail(std::error_code(errno, std::generic_category()));
    }
#endif
    return fail(errc::not_supported);
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
