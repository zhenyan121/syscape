#ifndef SYSCAPE_DETAIL_OS_REDOX_HPP
#define SYSCAPE_DETAIL_OS_REDOX_HPP

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
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

inline result<std::string> read_os_file(const char* path) {
    int fd = -1;
    for (;;) {
        fd = ::open(path, O_RDONLY | O_CLOEXEC);
        if (fd >= 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        const int err = errno;
        if (err == ENOENT) {
            return fail(errc::not_found);
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(err, std::generic_category()));
    }

    struct fd_guard {
        int descriptor;
        ~fd_guard() {
            if (descriptor >= 0) {
                ::close(descriptor);
            }
        }
    } guard {fd};

    std::string content;
    char buffer[1024];
    for (;;) {
        const ssize_t bytes = ::read(fd, buffer, sizeof(buffer));
        if (bytes > 0) {
            const auto byte_count = static_cast<std::size_t>(bytes);
            if (byte_count > 64U * 1024U - content.size()) {
                return fail(errc::value_too_large);
            }
            content.append(buffer, byte_count);
        } else if (bytes == 0) {
            break;
        } else if (errno == EINTR) {
            continue;
        } else {
            const int err = errno;
            if (err == EACCES || err == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(err, std::generic_category()));
        }
    }
    return content;
}

inline std::string find_ini_value(std::string_view content,
                                  std::string_view key) {
    std::string_view rem = content;
    while (!rem.empty()) {
        const auto nl = rem.find('\n');
        std::string_view line =
            (nl != std::string_view::npos) ? rem.substr(0U, nl) : rem;
        rem = (nl != std::string_view::npos) ? rem.substr(nl + 1U)
                                             : std::string_view();

        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' ||
                                 line.back() == '\t')) {
            line.remove_suffix(1U);
        }
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
            line.remove_prefix(1U);
        }
        if (line.rfind(key, 0) == 0 && line.size() > key.size() &&
            line[key.size()] == '=') {
            std::string_view val = line.substr(key.size() + 1U);
            while (!val.empty() && (val.front() == ' ' || val.front() == '"')) {
                val.remove_prefix(1U);
            }
            while (!val.empty() && (val.back() == ' ' || val.back() == '"' ||
                                    val.back() == '\r')) {
                val.remove_suffix(1U);
            }
            if (!val.empty()) {
                return std::string(val);
            }
        }
    }
    return {};
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
    const auto file = read_os_file("/etc/os-release");
    if (file) {
        const std::string name = find_ini_value(*file, "NAME");
        if (!name.empty()) {
            return name;
        }
    }
    return std::string("Redox OS");
}

inline result<std::string> product_version() {
    const auto file = read_os_file("/etc/os-release");
    if (file) {
        const std::string version_id = find_ini_value(*file, "VERSION_ID");
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
    if (name.version[0] != '\0') {
        return std::string(name.version);
    }
    if (name.release[0] != '\0') {
        return std::string(name.release);
    }
    return fail(errc::not_found);
}

inline result<std::string> parse_host_name(std::string_view content) {
    while (!content.empty() &&
           (content.back() == '\n' || content.back() == '\r' ||
            content.back() == ' ' || content.back() == '\t')) {
        content.remove_suffix(1U);
    }
    if (content.empty()) {
        return fail(errc::not_found);
    }
    for (const char c : content) {
        const auto uc = static_cast<unsigned char>(c);
        if (uc < 33 || uc == 127) {
            return fail(errc::malformed_data);
        }
    }
    return std::string(content);
}

inline result<std::string> parse_hostname(std::string_view content) {
    return parse_host_name(content);
}

struct system_hostname_reader {
    static result<std::string> read() {
        return read_os_file("/etc/hostname");
    }
};

template <typename HostnameReader = system_hostname_reader>
inline result<std::string> host_name() {
    // Redox relibc reads /etc/hostname into fixed-size utsname.nodename without
    // trimming trailing newlines. Read /etc/hostname directly to avoid silent
    // truncation and strip newlines.
    const auto file = HostnameReader::read();
    if (!file) {
        return fail(file.error());
    }
    return parse_host_name(*file);
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
