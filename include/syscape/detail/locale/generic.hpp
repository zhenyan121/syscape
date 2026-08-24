#ifndef SYSCAPE_DETAIL_LOCALE_GENERIC_HPP
#define SYSCAPE_DETAIL_LOCALE_GENERIC_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace locale_backend {

inline result<std::string> current_locale() {
    return fail(errc::not_supported);
}

inline result<std::string> text_encoding() {
    return fail(errc::not_supported);
}

inline result<std::int32_t> utc_offset_seconds() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> preferred_languages() {
    return fail(errc::not_supported);
}

inline result<std::string> country_region_code() {
    return fail(errc::not_supported);
}

inline result<std::string> time_zone_identifier() {
    return fail(errc::not_supported);
}

} // namespace locale_backend
} // namespace detail
} // namespace syscape

#endif
