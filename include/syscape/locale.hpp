#ifndef SYSCAPE_LOCALE_HPP
#define SYSCAPE_LOCALE_HPP

/// @file
/// @brief Hosted locale, text-encoding, language-preference, and time-zone
/// queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note Linux, macOS, FreeBSD, and Solaris share POSIX backends for locale
/// identity, text encoding, and UTC offset; Windows provides a native backend.
/// Language preferences and region codes use native backends on Windows and
/// macOS. Time-zone identifiers use the documented localtime configuration on
/// Linux and dynamic time-zone information on Windows. macOS reports
/// not_supported because CoreFoundation silently substitutes GMT when the
/// system zone is indeterminable. FreeBSD uses its POSIX locale facilities and
/// documented zoneinfo layout, while language preferences and region codes
/// report not_supported. Solaris reads /etc/timezone and zoneinfo files for
/// time zones, reporting language preferences and region codes as
/// not_supported. Other targets use the generic not-supported fallback. On
/// Android, text_encoding() requires API level 26 or later and reports
/// not_supported on earlier API levels.
/// @note On Windows the preference queries require _WIN32_WINNT and WINVER
/// declarations of at least 0x0600 (Windows Vista); a lower setting is
/// rejected with a diagnostic. When absent, the internal SDK include boundary
/// temporarily requests 0x0601 so this header cannot hide declarations needed
/// by other Syscape modules included later.
/// @note Expected failures are returned as native error codes where
/// available, or as syscape::errc values for missing, malformed, or
/// unsupported data.
/// @note Thread-safety: these queries observe C-runtime locale state,
/// process environment state (TZ and TZDIR on Linux), and platform user
/// preference configuration; concurrent changes to that state are an
/// unavoidable platform race and are documented as such rather than hidden.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/locale.hpp requires C++17 or later"
#endif

#include <cstdint>
#include <string>
#include <vector>

#include <syscape/detail/locale/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/locale/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/locale/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/locale/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/locale/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/locale/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/locale/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/locale/dragonfly.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__ANDROID__)
#include <syscape/detail/locale/android.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    (defined(__sun) || defined(__sun__) || defined(sun))
#include <syscape/detail/locale/solaris.hpp>
#else
#include <syscape/detail/locale/generic.hpp>
#endif

namespace syscape {
namespace locale {

/// Returns the current default locale identifier in effect for the calling
/// process.
///
/// The identifier is reported verbatim from the platform source and is never
/// normalized across platforms. The C runtime's locale string is reported
/// verbatim (for example "C", "zh_CN.UTF-8", or a composite form used when
/// individual categories differ). The value is a fresh snapshot of state
/// that the process itself may change, for example through std::setlocale; no
/// stability is guaranteed between calls.
/// @return A non-empty UTF-8 locale identifier, malformed_data for invalid
/// platform data, invalid_encoding when the native text is not valid UTF-8,
/// not_supported when the platform exposes no acceptable source, or a native
/// platform error.
inline result<std::string> current_locale() {
    return detail::locale_common::validate_utf8_label(
        detail::locale_backend::current_locale());
}

/// Returns the name of the text encoding currently used for non-Unicode
/// narrow text.
///
/// The label comes from the same process context as current_locale() and is
/// reported verbatim: POSIX systems report the codeset name selected by the
/// LC_CTYPE category (for example "UTF-8" or "ANSI_X3.4-1968"), while the
/// Microsoft C runtime reports its current multibyte code page as a decimal
/// identifier (for example "936"). The encoding can change when the process
/// changes its own locale; each call returns a fresh snapshot without caching.
/// @return A non-empty UTF-8 encoding label, malformed_data for invalid
/// platform data, invalid_encoding when the native text is not valid UTF-8,
/// not_supported when the platform exposes no acceptable source, or a native
/// platform error.
inline result<std::string> text_encoding() {
    return detail::locale_common::validate_utf8_label(
        detail::locale_backend::text_encoding());
}

/// Returns the local time zone's UTC offset in effect at the moment of the
/// query, in seconds east of UTC.
///
/// On POSIX systems the offset reflects the time-zone configuration visible
/// to the calling process, including any TZ environment influence; on Windows
/// it reflects the system-configured local time zone. Seasonal adjustments
/// that begin after the query are not reflected, so the value is a snapshot
/// and can change across transitions; zero is valid data for UTC itself.
/// @return An offset strictly between -86400 and 86400 seconds, not_found
/// when the platform exposes no determinable local time zone, malformed_data
/// for out-of-range platform values, not_supported when the platform exposes
/// no acceptable source, or a native platform error.
inline result<std::int32_t> utc_offset_seconds() {
    return detail::locale_common::validate_utc_offset_seconds(
        detail::locale_backend::utc_offset_seconds());
}

/// Returns the ordered list of language identifiers the user prefers.
///
/// Entries are reported verbatim in the platform's recorded preference
/// order and are never normalized across platforms: Windows reports its
/// documented user preferred UI language names (for example "zh-CN"),
/// while macOS reports the identifiers behind the user's language
/// preference list (for example "zh-Hans-CN"). Linux exposes no documented
/// system-level source for such a list and reports not_supported rather
/// than deriving entries from locale strings or message-translation
/// environment overrides. The list is a fresh snapshot of user
/// configuration that can change between calls; an entry order carries no
/// cross-platform meaning beyond preference ranking within one platform.
/// @return A non-empty list of non-empty UTF-8 language identifiers,
/// malformed_data when the platform records an empty or unusable list,
/// invalid_encoding when native text is not valid UTF-8, not_supported
/// when the platform exposes no acceptable source, or a native platform
/// error.
inline result<std::vector<std::string>> preferred_languages() {
    return detail::locale_common::validate_language_list(
        detail::locale_backend::preferred_languages());
}

/// Returns the country or region code recorded by the platform's user
/// configuration.
///
/// The code is reported verbatim from the same configuration context on
/// every platform: Windows reports the ISO 3166 region of the user locale
/// and macOS reports the country code of the current locale, so both
/// describe the configured locale's region rather than a separately
/// administered geographic setting. Linux records no separate region fact
/// and reports not_supported because decomposing the locale identifier
/// would fabricate structure the platform does not expose. The value can
/// change when the user reconfigures the locale; each call returns a fresh
/// snapshot without caching.
/// @return A non-empty UTF-8 region code, not_found when the platform
/// records no region for the current configuration, malformed_data for
/// unusable platform data, invalid_encoding when the native text is not
/// valid UTF-8, not_supported when the platform exposes no acceptable
/// source, or a native platform error.
inline result<std::string> country_region_code() {
    return detail::locale_common::validate_utf8_label(
        detail::locale_backend::country_region_code());
}

/// Returns the identifier of the local time zone as recorded by the
/// platform.
///
/// Identifiers preserve each platform's own vocabulary verbatim and are
/// deliberately not normalized across platforms: Linux extracts the
/// identifier from the documented localtime configuration link (for
/// example "Asia/Shanghai", with "UTC" reported for the documented missing
/// default), while Windows reports the dynamic time-zone registry-key name
/// (for example "China Standard Time"), which names a Windows zone database
/// entry rather than an IANA identifier. macOS reports not_supported because
/// CoreFoundation's GMT fallback cannot be distinguished from a genuinely
/// configured GMT zone. On Linux an explicitly empty TZ selects and reports
/// "UTC", while a geographical TZ file name is recognized with or without
/// its optional leading colon. A POSIX-style TZ rule string records no
/// identifier, so the query reports not_found there instead of rendering a
/// fabricated name. The value reflects the platform configuration source
/// documented above and can change when that source changes; where both
/// queries are supported, it is not guaranteed to match any particular
/// instant's offset from utc_offset_seconds() unless queried at the same
/// moment under the same configuration.
/// @return A non-empty UTF-8 time-zone identifier, not_found when the
/// effective configuration records no extractable identifier, malformed_data
/// for unusable platform data, invalid_encoding when the native text is not
/// valid UTF-8, not_supported when the platform exposes no acceptable
/// source, or a native platform error.
inline result<std::string> time_zone_identifier() {
    return detail::locale_common::validate_utf8_label(
        detail::locale_backend::time_zone_identifier());
}

} // namespace locale
} // namespace syscape

#endif
