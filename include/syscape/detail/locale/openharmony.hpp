#ifndef SYSCAPE_DETAIL_LOCALE_OPENHARMONY_HPP
#define SYSCAPE_DETAIL_LOCALE_OPENHARMONY_HPP

#include <string>
#include <vector>

#include <syscape/detail/locale/posix.hpp>
#include <syscape/detail/openharmony/parameter.hpp>
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
        openharmony::get_parameter("persist.sys.timezone");
    if (tz && !tz->empty()) {
        return tz;
    }
    if (!tz && tz.error() != errc::not_found &&
        tz.error() != errc::not_supported) {
        return fail(tz.error());
    }

    const result<std::string> tz2 =
        openharmony::get_parameter("persist.time.timezone");
    if (tz2 && !tz2->empty()) {
        return tz2;
    }
    if (!tz2 && tz2.error() != errc::not_found &&
        tz2.error() != errc::not_supported) {
        return fail(tz2.error());
    }

    return fail(errc::not_supported);
}

} // namespace locale_backend
} // namespace detail
} // namespace syscape

#endif
