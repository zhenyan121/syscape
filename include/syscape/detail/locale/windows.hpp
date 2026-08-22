#ifndef SYSCAPE_DETAIL_LOCALE_WINDOWS_HPP
#define SYSCAPE_DETAIL_LOCALE_WINDOWS_HPP

#include <clocale>
#include <cstdint>
#include <string>
#include <system_error>

#include <mbctype.h>
#include <windows.h>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace locale_backend {

inline std::error_code last_error() noexcept {
    return std::error_code(static_cast<int>(::GetLastError()),
                           std::system_category());
}

/// Renders a C-runtime multibyte code-page identifier as its decimal label.
///
/// Windows identifies text encodings numerically, so the portable string
/// contract carries the decimal rendering of the native identifier verbatim.
inline std::string code_page_label(::UINT page) {
    return std::to_string(static_cast<unsigned long>(page));
}

/// Combines a time-zone bias with the bias component currently in force.
///
/// Windows expresses biases in minutes west of UTC, so the portable offset
/// negates and widens the sum. The sum of documented zone components cannot
/// approach one day, but the guard keeps malformed platform data from
/// becoming a fabricated offset.
inline result<std::int32_t> active_offset_seconds(
    ::LONG base_bias_minutes, ::LONG adjustment_bias_minutes) {
    constexpr std::int32_t seconds_per_day = 24 * 60 * 60;
    const std::int64_t total_minutes =
        static_cast<std::int64_t>(base_bias_minutes) +
        static_cast<std::int64_t>(adjustment_bias_minutes);
    const std::int64_t seconds =
        -total_minutes * 60;
    if (seconds <= -seconds_per_day || seconds >= seconds_per_day) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::int32_t>(seconds);
}

/// Returns the process's current locale string reported by the C runtime.
inline result<std::string> current_locale() {
    const char* const name = std::setlocale(LC_ALL, nullptr);
    if (name == nullptr) { return fail(errc::malformed_data); }
    return std::string(name);
}

/// Returns the code page used by the C runtime's multibyte-text functions.
inline result<std::string> text_encoding() {
    return code_page_label(static_cast<::UINT>(::_getmbcp()));
}

/// Returns the local time zone's UTC offset in effect right now.
///
/// GetTimeZoneInformation reports which seasonal adjustment applies at the
/// moment of the call; TIME_ZONE_ID_UNKNOWN still carries a valid base bias
/// for zones without seasonal adjustments. A dynamic time zone resolves
/// through the same interface for the current instant.
inline result<std::int32_t> utc_offset_seconds() {
    ::TIME_ZONE_INFORMATION information {};
    const ::DWORD state = ::GetTimeZoneInformation(&information);
    if (state == TIME_ZONE_ID_INVALID) { return fail(last_error()); }

    ::LONG adjustment = 0;
    if (state == TIME_ZONE_ID_STANDARD) {
        adjustment = information.StandardBias;
    } else if (state == TIME_ZONE_ID_DAYLIGHT) {
        adjustment = information.DaylightBias;
    }
    return active_offset_seconds(information.Bias, adjustment);
}

} // namespace locale_backend
} // namespace detail
} // namespace syscape

#endif
