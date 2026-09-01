#ifndef SYSCAPE_DETAIL_LOCALE_ANDROID_HPP
#define SYSCAPE_DETAIL_LOCALE_ANDROID_HPP

#include <string>
#include <vector>

#include <syscape/detail/android/property.hpp>
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
    const result<std::string> tz =
        android::get_property("persist.sys.timezone");
    if (tz && !tz->empty()) {
        return tz;
    }
    return fail(errc::not_supported);
}

} // namespace locale_backend
} // namespace detail
} // namespace syscape

#endif
