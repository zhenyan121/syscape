#ifndef SYSCAPE_DETAIL_LOCALE_AIX_HPP
#define SYSCAPE_DETAIL_LOCALE_AIX_HPP

#include <cstdlib>
#include <string>
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

inline result<std::string> time_zone_identifier() {
    char buf[512] = {};
    const ssize_t len = ::readlink("/etc/localtime", buf, sizeof(buf) - 1U);
    if (len > 0) {
        buf[len] = '\0';
        std::string path(buf);
        const std::string marker = "zoneinfo/";
        const auto pos = path.find(marker);
        if (pos != std::string::npos) {
            return path.substr(pos + marker.size());
        }
    }

    const char* const tz = std::getenv("TZ");
    if (tz != nullptr && tz[0] != '\0') {
        return std::string(tz);
    }
    return fail(errc::not_supported);
}

} // namespace locale_backend
} // namespace detail
} // namespace syscape

#endif
