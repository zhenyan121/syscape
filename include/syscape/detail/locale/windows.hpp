#ifndef SYSCAPE_DETAIL_LOCALE_WINDOWS_HPP
#define SYSCAPE_DETAIL_LOCALE_WINDOWS_HPP

// The locale preference and time-zone identity queries use declarations
// introduced in Windows Vista (GetUserPreferredUILanguages,
// GetLocaleInfoEx, and GetDynamicTimeZoneInformation). A lower
// _WIN32_WINNT or WINVER setting is rejected with a diagnostic; when the
// macros are absent they are scoped across the internal Windows SDK include
// boundary below. That boundary requests the repository's current highest
// Windows declaration level (Windows 7) so including this header first cannot
// hide declarations needed by another Syscape header included later. The
// locale APIs themselves retain their documented Windows Vista minimum.
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0600
#error "syscape/locale.hpp requires _WIN32_WINNT >= 0x0600 on Windows"
#endif
#if defined(WINVER) && WINVER < 0x0600
#error "syscape/locale.hpp requires WINVER >= 0x0600 on Windows"
#endif

#include <clocale>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if !defined(_WIN32_WINNT)
#define SYSCAPE_DETAIL_LOCALE_DEFINED_WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#if !defined(WINVER)
#define SYSCAPE_DETAIL_LOCALE_DEFINED_WINVER
#define WINVER 0x0601
#endif

#include <mbctype.h>
#include <windows.h>

#if defined(SYSCAPE_DETAIL_LOCALE_DEFINED_WIN32_WINNT)
#undef _WIN32_WINNT
#undef SYSCAPE_DETAIL_LOCALE_DEFINED_WIN32_WINNT
#endif
#if defined(SYSCAPE_DETAIL_LOCALE_DEFINED_WINVER)
#undef WINVER
#undef SYSCAPE_DETAIL_LOCALE_DEFINED_WINVER
#endif

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace locale_backend {

inline std::error_code last_error() noexcept {
    return std::error_code(static_cast<int>(::GetLastError()),
                           std::system_category());
}

/// Converts one native UTF-16 string into the portable UTF-8 contract.
inline result<std::string> wide_to_utf8(std::wstring_view value) {
    static_assert(sizeof(wchar_t) == sizeof(char16_t),
                  "The Windows backend requires 16-bit wchar_t");
    std::u16string converted;
    converted.reserve(value.size());
    for (const wchar_t unit : value) {
        converted.push_back(static_cast<char16_t>(unit));
    }
    return utf16_to_utf8(converted);
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

/// Splits one documented NUL-delimited preferred-languages buffer.
///
/// GetUserPreferredUILanguages renders its ordered list null-delimited and
/// double-null terminated. The first empty segment ends the recorded list;
/// everything after it is buffer padding. A segment that cannot be
/// converted to UTF-8 fails the query instead of producing corrupted text.
inline result<std::vector<std::string>> split_language_names(
    const wchar_t* buffer, std::size_t units) {
    std::vector<std::string> languages;
    std::size_t begin = 0U;
    while (begin < units) {
        std::size_t end = begin;
        while (end < units && buffer[end] != L'\0') { ++end; }
        if (end == begin) { break; }
        std::u16string segment;
        segment.reserve(end - begin);
        for (std::size_t index = begin; index < end; ++index) {
            segment.push_back(static_cast<char16_t>(buffer[index]));
        }
        result<std::string> language = utf16_to_utf8(segment);
        if (!language) { return fail(language.error()); }
        languages.push_back(std::move(*language));
        begin = end + 1U;
    }
    return languages;
}

/// Returns the ordered list of language identifiers the user prefers,
/// reported verbatim from the platform's own vocabulary.
inline result<std::vector<std::string>> preferred_languages() {
    ::ULONG count = 0;
    ::ULONG size = 0;
    if (!::GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, nullptr,
                                       &size)) {
        return fail(last_error());
    }
    if (size == 0U) { return fail(errc::malformed_data); }
    std::wstring buffer(static_cast<std::size_t>(size), L'\0');
    if (!::GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count,
                                       &buffer[0], &size)) {
        return fail(last_error());
    }
    return split_language_names(buffer.data(),
                                static_cast<std::size_t>(size));
}

/// Returns the country or region code recorded by the user locale,
/// reported verbatim from the platform's own vocabulary.
///
/// GetLocaleInfoEx answers both sizing calls with the character count
/// including the terminating null; a mismatch between the two calls is a
/// torn snapshot reported as malformed platform data.
inline result<std::string> country_region_code() {
    const int sized = ::GetLocaleInfoEx(
        LOCALE_NAME_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, nullptr, 0);
    if (sized <= 0) { return fail(last_error()); }
    std::wstring buffer(static_cast<std::size_t>(sized), L'\0');
    const int written = ::GetLocaleInfoEx(
        LOCALE_NAME_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, &buffer[0],
        sized);
    if (written == 0) { return fail(last_error()); }
    if (written != sized) { return fail(errc::malformed_data); }
    while (!buffer.empty() && buffer.back() == L'\0') { buffer.pop_back(); }
    return wide_to_utf8(buffer);
}

/// Extracts one time-zone registry-key name from its fixed-size record.
///
/// DYNAMIC_TIME_ZONE_INFORMATION stores the key name in a fixed-size field
/// that must be terminated within the documented capacity; an unterminated
/// or empty recording is malformed platform data rather than truncated
/// text.
inline result<std::string> time_zone_key_name(
    const wchar_t* field, std::size_t capacity) {
    std::size_t length = 0U;
    while (length < capacity && field[length] != L'\0') { ++length; }
    if (length == capacity || length == 0U) {
        return fail(errc::malformed_data);
    }
    return wide_to_utf8(std::wstring_view(field, length));
}

/// Returns the identifier of the system-configured local time zone,
/// reported verbatim as the documented registry-key name rather than being
/// normalized to another platform's vocabulary.
inline result<std::string> time_zone_identifier() {
    ::DYNAMIC_TIME_ZONE_INFORMATION information {};
    const ::DWORD state =
        ::GetDynamicTimeZoneInformation(&information);
    if (state == TIME_ZONE_ID_INVALID) { return fail(last_error()); }
    return time_zone_key_name(
        information.TimeZoneKeyName,
        sizeof(information.TimeZoneKeyName) /
            sizeof(information.TimeZoneKeyName[0]));
}

} // namespace locale_backend
} // namespace detail
} // namespace syscape

#endif
