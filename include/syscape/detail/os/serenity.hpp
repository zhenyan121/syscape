#ifndef SYSCAPE_DETAIL_OS_SERENITY_HPP
#define SYSCAPE_DETAIL_OS_SERENITY_HPP

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <time.h>
#include <unistd.h>
#include <vector>

#include <sys/utsname.h>

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

inline result<std::chrono::milliseconds> monotonic_uptime() {
    struct ::timespec value {};
    if (::clock_gettime(CLOCK_MONOTONIC, &value) != 0) {
        const int saved_errno = errno;
        if (saved_errno == EACCES || saved_errno == EPERM) {
            return fail(errc::permission_denied);
        }
        if (saved_errno == 0) {
            return fail(errc::io_error);
        }
        return fail(std::error_code(saved_errno, std::generic_category()));
    }
    if (value.tv_sec < 0 || value.tv_nsec < 0 || value.tv_nsec >= 1000000000L) {
        return fail(errc::malformed_data);
    }
    const auto seconds = static_cast<std::uint64_t>(value.tv_sec);
    const auto maximum_milliseconds =
        static_cast<std::uint64_t>((std::chrono::milliseconds::max)().count());
    if (seconds > maximum_milliseconds / 1000U) {
        return fail(errc::value_too_large);
    }
    const auto milliseconds =
        seconds * 1000U + static_cast<std::uint64_t>(value.tv_nsec / 1000000L);
    if (milliseconds > maximum_milliseconds) {
        return fail(errc::value_too_large);
    }
    return std::chrono::milliseconds(
        static_cast<std::chrono::milliseconds::rep>(milliseconds));
}

inline result<std::string> product_name() {
    return std::string("SerenityOS");
}

inline result<std::string> product_version() {
    FILE* fp = std::fopen("/etc/os-release", "r");
    if (fp != nullptr) {
        std::string line;
        std::string version_id;
        constexpr std::string_view prefix = "VERSION_ID=";
        while (read_line(fp, line)) {
            if (line.rfind(prefix, 0) == 0) {
                std::size_t val = prefix.size();
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
    return std::string("SerenityOS");
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
            const int saved_errno = errno;
            if (saved_errno == EACCES || saved_errno == EPERM) {
                return fail(errc::permission_denied);
            }
            if (saved_errno == 0) {
                return fail(errc::io_error);
            }
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        buffer.resize(buffer.size() * 2U);
    }
    return fail(errc::value_too_large);
}

inline result<std::string> boot_identifier() {
    return fail(errc::not_supported);
}

inline result<std::chrono::system_clock::time_point> boot_time() {
    const auto elapsed = monotonic_uptime();
    if (!elapsed) {
        return fail(elapsed.error());
    }
    const auto now = std::chrono::system_clock::now();
    const auto duration =
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            *elapsed);
    if (duration > now.time_since_epoch()) {
        return fail(errc::malformed_data);
    }
    return now - duration;
}

inline result<std::chrono::milliseconds> uptime() {
    return monotonic_uptime();
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
