#ifndef SYSCAPE_DETAIL_LOCALE_HURD_HPP
#define SYSCAPE_DETAIL_LOCALE_HURD_HPP

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <sys/stat.h>

#include <syscape/detail/locale/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace locale_backend {

class owned_fd {
    public:
    explicit owned_fd(int fd) noexcept : fd_(fd) {}
    ~owned_fd() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    owned_fd(const owned_fd&) = delete;
    owned_fd& operator=(const owned_fd&) = delete;
    int get() const noexcept {
        return fd_;
    }

    private:
    int fd_;
};

inline result<std::string> validate_zone_identifier(std::string_view value) {
    if (value.empty() || value.front() == '/' || value.back() == '/' ||
        value.find('\0') != std::string_view::npos) {
        return fail(errc::malformed_data);
    }
    std::size_t offset = 0U;
    while (offset < value.size()) {
        const std::size_t end = value.find('/', offset);
        const std::string_view part = end == std::string_view::npos
                                          ? value.substr(offset)
                                          : value.substr(offset, end - offset);
        if (part.empty() || part == "." || part == "..") {
            return fail(errc::malformed_data);
        }
        if (end == std::string_view::npos) {
            break;
        }
        offset = end + 1U;
    }
    return std::string(value);
}

inline result<void> validate_zone_file(std::string_view identifier) {
    const std::string path =
        std::string("/usr/share/zoneinfo/") + std::string(identifier);
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    const owned_fd guard(fd);
    struct ::stat st {};
    if (::fstat(fd, &st) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (!S_ISREG(st.st_mode)) {
        return fail(errc::malformed_data);
    }
    char magic[4];
    std::size_t used = 0U;
    while (used < sizeof(magic)) {
        const ssize_t count = ::read(fd, magic + used, sizeof(magic) - used);
        if (count > 0) {
            used += static_cast<std::size_t>(count);
        } else if (count == 0) {
            return fail(errc::malformed_data);
        } else if (errno != EINTR) {
            return fail(std::error_code(errno, std::generic_category()));
        }
    }
    if (std::memcmp(magic, "TZif", sizeof(magic)) != 0) {
        return fail(errc::malformed_data);
    }
    return {};
}

inline result<std::vector<std::string>> preferred_languages() {
    return fail(errc::not_supported);
}

inline result<std::string> country_region_code() {
    return fail(errc::not_supported);
}

inline result<std::string> time_zone_identifier() {
    const char* const tz = std::getenv("TZ");
    if (tz != nullptr) {
        std::string_view spec(tz);
        if (spec.empty() || spec == ":") {
            return std::string("UTC");
        }
        if (spec.front() == ':') {
            spec.remove_prefix(1U);
        }
        if (spec.find(',') != std::string_view::npos) {
            return fail(errc::not_found);
        }
        auto valid_id = validate_zone_identifier(spec);
        if (!valid_id) {
            return fail(errc::not_found);
        }
        const auto valid_file = validate_zone_file(*valid_id);
        if (!valid_file) {
            return fail(valid_file.error());
        }
        return *valid_id;
    }

    std::size_t size = 256U;
    constexpr std::size_t max_size = 64U * 1024U;
    while (size <= max_size) {
        std::string buf(size, '\0');
        errno = 0;
        const ssize_t len = ::readlink("/etc/localtime", &buf[0], size);
        if (len > 0) {
            if (static_cast<std::size_t>(len) < size) {
                buf.resize(static_cast<std::size_t>(len));
                constexpr std::string_view marker = "zoneinfo/";
                const auto pos = buf.find(marker);
                if (pos != std::string::npos) {
                    std::string_view id_view(buf.data() + pos + marker.size(),
                                             buf.size() - pos - marker.size());
                    auto valid_id = validate_zone_identifier(id_view);
                    if (!valid_id) {
                        return fail(valid_id.error());
                    }
                    const auto valid_file = validate_zone_file(*valid_id);
                    if (!valid_file) {
                        return fail(valid_file.error());
                    }
                    return *valid_id;
                }
                return fail(errc::not_found);
            }
            size *= 2U;
            continue;
        }
        const int err = errno;
        if (err == ENOENT) {
            return fail(errc::not_found);
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err != 0 && err != EINVAL) {
            return fail(std::error_code(err, std::generic_category()));
        }
        break;
    }

    return fail(errc::not_found);
}

} // namespace locale_backend
} // namespace detail
} // namespace syscape

#endif
