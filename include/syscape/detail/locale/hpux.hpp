#ifndef SYSCAPE_DETAIL_LOCALE_HPUX_HPP
#define SYSCAPE_DETAIL_LOCALE_HPUX_HPP

#include <cerrno>
#include <cstdlib>
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

inline result<std::string> zoneinfo_identifier_from_link(const char* path) {
    std::size_t size = 256U;
    constexpr std::size_t max_size = 1024U * 1024U;
    while (size <= max_size) {
        std::string target(size, '\0');
        errno = 0;
        const ssize_t length = ::readlink(path, &target[0], target.size());
        if (length < 0) {
            const int saved_errno = errno;
            if (saved_errno == ENOENT || saved_errno == EINVAL) {
                return fail(errc::not_found);
            }
            if (saved_errno == EACCES || saved_errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        if (length == 0) {
            return fail(errc::malformed_data);
        }
        if (static_cast<std::size_t>(length) == target.size()) {
            size *= 2U;
            continue;
        }
        target.resize(static_cast<std::size_t>(length));
        const std::string marker = "zoneinfo/";
        const auto pos = target.find(marker);
        if (pos != std::string::npos) {
            const std::string identifier = target.substr(pos + marker.size());
            if (identifier.empty()) {
                return fail(errc::malformed_data);
            }
            return identifier;
        }
        return fail(errc::not_found);
    }
    return fail(errc::value_too_large);
}

inline result<std::string> zoneinfo_identifier_from_tz(const char* configured) {
    std::string value(configured);
    if (value.empty() || value == ":") {
        return std::string("UTC");
    }

    const bool explicit_file = value.front() == ':';
    if (explicit_file) {
        value.erase(0U, 1U);
    }

    constexpr std::string_view hpux_root = "/usr/share/lib/zoneinfo/";
    constexpr std::string_view alternate_root = "/usr/share/zoneinfo/";
    if (value.compare(0U, hpux_root.size(), hpux_root) == 0) {
        value.erase(0U, hpux_root.size());
    } else if (value.compare(0U, alternate_root.size(), alternate_root) == 0) {
        value.erase(0U, alternate_root.size());
    }

    if (value.empty() || value.front() == '/' || value.back() == '/') {
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

    if (!explicit_file && value.find('/') == std::string::npos &&
        value.find_first_of("0123456789,+-") != std::string::npos) {
        return fail(errc::not_found);
    }
    return value;
}

inline result<std::string> time_zone_identifier() {
    const char* const tz = std::getenv("TZ");
    if (tz != nullptr) {
        return zoneinfo_identifier_from_tz(tz);
    }

    const auto linked_identifier =
        zoneinfo_identifier_from_link("/etc/localtime");
    if (linked_identifier) {
        return linked_identifier;
    }
    if (linked_identifier.error() != errc::not_found) {
        return fail(linked_identifier.error());
    }

    return fail(errc::not_supported);
}

} // namespace locale_backend
} // namespace detail
} // namespace syscape

#endif
