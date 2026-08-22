#ifndef SYSCAPE_DETAIL_LOCALE_COMMON_HPP
#define SYSCAPE_DETAIL_LOCALE_COMMON_HPP

#include <cstdint>
#include <string>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace locale_common {

/// Validates a locale or encoding label reported by a platform backend.
///
/// A label must be non-empty and valid UTF-8. An empty label carries no
/// usable information and is malformed platform data rather than valid data,
/// because every supported platform source reports at least one character.
inline result<std::string> validate_utf8_label(result<std::string> value) {
    if (!value) { return fail(value.error()); }
    if (value->empty()) { return fail(errc::malformed_data); }
    if (!is_valid_utf8(*value)) { return fail(errc::invalid_encoding); }
    return value;
}

/// Validates a UTC offset reported by a platform backend.
///
/// A local time-zone offset must lie within one day of zero; any platform
/// value outside that range is malformed platform data instead of a real
/// zone offset. Zero is valid data (UTC itself), never an error sentinel.
inline result<std::int32_t> validate_utc_offset_seconds(
    result<std::int32_t> value) {
    if (!value) { return fail(value.error()); }
    constexpr std::int32_t seconds_per_day = 24 * 60 * 60;
    if (*value <= -seconds_per_day || *value >= seconds_per_day) {
        return fail(errc::malformed_data);
    }
    return value;
}

} // namespace locale_common
} // namespace detail
} // namespace syscape

#endif
