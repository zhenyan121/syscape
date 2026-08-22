#ifndef SYSCAPE_DETAIL_LOCALE_POSIX_HPP
#define SYSCAPE_DETAIL_LOCALE_POSIX_HPP

#include <cerrno>
#include <clocale>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>
#include <system_error>

#include <langinfo.h>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace locale_backend {

/// Parses a strftime "%z" UTC-offset rendering into seconds east of UTC.
///
/// The accepted forms are the ISO 8601 basic renderings produced by
/// strftime: a sign followed by two hour digits and two minute digits,
/// optionally extended by two second digits. Any other shape, digit range,
/// or character is malformed platform data rather than a real offset.
inline result<std::int32_t> parse_gmt_offset_text(std::string_view text) {
    if (text.size() != 5U && text.size() != 7U) {
        return fail(errc::malformed_data);
    }

    const bool negative = text.front() == '-';
    const bool positive = text.front() == '+';
    if (!negative && !positive) { return fail(errc::malformed_data); }

    const auto digit = [&text](std::size_t index) noexcept -> int {
        const char character = text[index];
        return character >= '0' && character <= '9'
                   ? static_cast<int>(character - '0')
                   : -1;
    };

    const int hours = digit(1U) * 10 + digit(2U);
    const int minutes = digit(3U) * 10 + digit(4U);
    const bool extended = text.size() == 7U;
    int seconds = 0;
    if (extended) { seconds = digit(5U) * 10 + digit(6U); }
    if (digit(1U) < 0 || digit(2U) < 0 || digit(3U) < 0 || digit(4U) < 0 ||
        (extended && (digit(5U) < 0 || digit(6U) < 0)) ||
        hours > 23 || minutes > 59 || seconds > 59) {
        return fail(errc::malformed_data);
    }

    const std::int32_t magnitude =
        static_cast<std::int32_t>(hours * 3600 + minutes * 60 + seconds);
    return negative ? -magnitude : magnitude;
}

/// Returns the process's current locale string reported by the C runtime.
///
/// The query form of setlocale never modifies state. When process categories
/// differ, the C runtime may report its composite rendering of every
/// category; that rendering is platform data and is copied verbatim.
inline result<std::string> current_locale() {
    const char* const name = std::setlocale(LC_ALL, nullptr);
    // A query cannot fail per the C standard; an absent name would still be
    // unusable data, so it is rejected instead of fabricated.
    if (name == nullptr) { return fail(errc::malformed_data); }
    return std::string(name);
}

/// Returns the codeset name selected by the process's LC_CTYPE category.
///
/// nl_langinfo copies nothing, so the returned static text is duplicated
/// immediately. An empty recording is rejected by the public boundary as
/// malformed platform data.
inline result<std::string> text_encoding() {
    const char* const name = ::nl_langinfo(CODESET);
    return name != nullptr ? std::string(name) : std::string();
}

/// Returns the local time zone's UTC offset in effect right now.
///
/// tzset refreshes the zone rules before the conversion because POSIX does
/// not require localtime_r to do so. strftime("%z") renders the offset for
/// the queried instant, including any seasonal adjustment in force. A
/// rendering that the C runtime declines to produce means the platform
/// exposes no determinable local time zone, which is reported as not_found.
inline result<std::int32_t> utc_offset_seconds() {
    ::tzset();

    const std::time_t now = std::time(nullptr);
    if (now == static_cast<std::time_t>(-1)) {
        return fail(std::error_code(EOVERFLOW, std::generic_category()));
    }

    std::tm local {};
    if (::localtime_r(&now, &local) == nullptr) {
        return fail(std::error_code(errno, std::generic_category()));
    }

    char formatted[16];
    const std::size_t length =
        std::strftime(formatted, sizeof(formatted), "%z", &local);
    if (length == 0U) { return fail(errc::not_found); }
    return parse_gmt_offset_text(
        std::string_view(formatted, static_cast<std::size_t>(length)));
}

} // namespace locale_backend
} // namespace detail
} // namespace syscape

#endif
