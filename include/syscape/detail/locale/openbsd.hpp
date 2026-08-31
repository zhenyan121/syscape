#ifndef SYSCAPE_DETAIL_LOCALE_OPENBSD_HPP
#define SYSCAPE_DETAIL_LOCALE_OPENBSD_HPP

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/detail/locale/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace locale_backend {

inline result<std::vector<std::string>> preferred_languages() {
    return fail(errc::not_supported);
}

inline result<std::string> country_region_code() {
    return fail(errc::not_supported);
}

class file_descriptor {
    public:
    explicit file_descriptor(int value) noexcept : value_(value) {}
    file_descriptor(const file_descriptor&) = delete;
    file_descriptor& operator=(const file_descriptor&) = delete;
    ~file_descriptor() {
        if (value_ >= 0) {
            static_cast<void>(::close(value_));
        }
    }

    int get() const noexcept {
        return value_;
    }

    private:
    int value_;
};

inline result<std::string> validate_zone_identifier(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.pop_back();
    }
    constexpr std::string_view root = "/usr/share/zoneinfo/";
    if (value.compare(0U, root.size(), root) == 0) {
        value.erase(0U, root.size());
    }
    if (value.empty() || value.front() == '/' || value.back() == '/' ||
        value.find('\0') != std::string::npos) {
        return fail(errc::malformed_data);
    }
    std::size_t offset = 0U;
    while (offset < value.size()) {
        const std::size_t end = value.find('/', offset);
        const std::string_view part(
            value.data() + offset,
            end == std::string::npos ? value.size() - offset : end - offset);
        if (part.empty() || part == "." || part == "..") {
            return fail(errc::malformed_data);
        }
        if (end == std::string::npos) {
            break;
        }
        offset = end + 1U;
    }
    return value;
}

inline result<void> validate_zone_file(std::string_view identifier) {
    const std::string path =
        std::string("/usr/share/zoneinfo/") + std::string(identifier);
    const int descriptor = ::open(path.c_str(), O_RDONLY);
    if (descriptor < 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    const file_descriptor owned_descriptor(descriptor);
    char magic[4];
    std::size_t used = 0U;
    while (used < sizeof(magic)) {
        const ssize_t count =
            ::read(owned_descriptor.get(), magic + used, sizeof(magic) - used);
        if (count > 0) {
            used += static_cast<std::size_t>(count);
        } else if (count == 0) {
            return fail(errc::malformed_data);
        } else if (errno != EINTR) {
            return fail(std::error_code(errno, std::generic_category()));
        }
    }
    return std::string_view(magic, sizeof(magic)) == "TZif"
               ? result<void>()
               : result<void>(fail(errc::malformed_data));
}

inline result<std::string> checked_zone_identifier(std::string value) {
    result<std::string> identifier = validate_zone_identifier(std::move(value));
    if (!identifier) {
        return fail(identifier.error());
    }
    const result<void> valid_file = validate_zone_file(*identifier);
    if (!valid_file) {
        return fail(valid_file.error());
    }
    return identifier;
}

inline result<std::string> time_zone_identifier() {
    const char* configured = ::getenv("TZ");
    if (configured != nullptr) {
        std::string value(configured);
        if (value.empty() || value == ":") {
            return std::string("UTC");
        }
        if (value.front() == ':') {
            value.erase(0U, 1U);
        }
        return checked_zone_identifier(std::move(value));
    }

    char target[1024];
    const ssize_t count =
        ::readlink("/etc/localtime", target, sizeof(target) - 1U);
    if (count > 0) {
        target[count] = '\0';
        return checked_zone_identifier(
            std::string(target, static_cast<std::size_t>(count)));
    }

    return fail(errc::not_found);
}

} // namespace locale_backend
} // namespace detail
} // namespace syscape

#endif
