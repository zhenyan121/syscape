#ifndef SYSCAPE_DETAIL_LOCALE_HAIKU_HPP
#define SYSCAPE_DETAIL_LOCALE_HAIKU_HPP

#include <fcntl.h>
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
    ssize_t len = ::readlink("/etc/localtime", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        std::string path(buf);
        const std::string marker = "zoneinfo/";
        const auto pos = path.find(marker);
        if (pos != std::string::npos) {
            return path.substr(pos + marker.size());
        }
    }

    const int fd = ::open("/boot/system/settings/Timezone", O_RDONLY);
    if (fd >= 0) {
        len = ::read(fd, buf, sizeof(buf) - 1);
        ::close(fd);
        if (len > 0) {
            buf[len] = '\0';
            std::string tz(buf);
            while (!tz.empty() && (tz.back() == '\n' || tz.back() == '\r' ||
                                   tz.back() == ' ')) {
                tz.pop_back();
            }
            if (!tz.empty()) {
                return tz;
            }
        }
    }
    return fail(errc::not_supported);
}

} // namespace locale_backend
} // namespace detail
} // namespace syscape

#endif
