#ifndef SYSCAPE_LOCALE_HPP
#define SYSCAPE_LOCALE_HPP

/// @file
/// @brief Hosted locale, text-encoding, and time-zone offset queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note Linux and macOS share a POSIX backend; Windows provides a native
/// backend. Other targets use the generic not-supported fallback.
/// @note Expected failures are returned as native error codes where
/// available, or as syscape::errc values for missing, malformed, or
/// unsupported data.
/// @note Thread-safety: these queries observe C-runtime locale and time-zone
/// state; concurrent changes to that state are an unavoidable platform race
/// and are documented as such rather than hidden.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/locale.hpp requires C++17 or later"
#endif

#include <cstdint>
#include <string>

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

} // namespace locale
} // namespace syscape

#endif
