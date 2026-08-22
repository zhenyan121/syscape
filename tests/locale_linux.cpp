#include <cctype>
#include <cerrno>
#include <clocale>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <system_error>
#include <time.h>

#include <langinfo.h>

#include <syscape/detail/locale/common.hpp>
#include <syscape/detail/locale/linux.hpp>
#include <syscape/detail/locale/posix.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/locale.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_offset_parsing() {
    const auto plus_eight =
        syscape::detail::locale_backend::parse_gmt_offset_text("+0800");
    expect(plus_eight && *plus_eight == 28800,
           "A +0800 rendering must parse to 28800 seconds");

    const auto minus_five_thirty =
        syscape::detail::locale_backend::parse_gmt_offset_text("-0530");
    expect(minus_five_thirty && *minus_five_thirty == -19800,
           "A -0530 rendering must parse to -19800 seconds");

    const auto zero =
        syscape::detail::locale_backend::parse_gmt_offset_text("+0000");
    expect(zero && *zero == 0,
           "A +0000 rendering must parse to zero, a valid offset");

    const auto extended =
        syscape::detail::locale_backend::parse_gmt_offset_text("+010101");
    expect(extended && *extended == 3661,
           "An extended +010101 rendering must parse to 3661 seconds");

    const auto negative_extended =
        syscape::detail::locale_backend::parse_gmt_offset_text("-020202");
    expect(negative_extended && *negative_extended == -7322,
           "A negative extended rendering keeps its sign");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("") &&
               syscape::detail::locale_backend::parse_gmt_offset_text("")
                       .error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "An empty rendering is malformed platform data");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("+800"),
           "A truncated rendering is malformed platform data");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("0800"),
           "A rendering without a sign is malformed platform data");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("+2400"),
           "Hours beyond 23 are malformed platform data");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("+0060"),
           "Minutes beyond 59 are malformed platform data");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("+000060")
               || syscape::detail::locale_backend::parse_gmt_offset_text(
                       "+000060")
                          .error() ==
                      syscape::make_error_code(
                          syscape::errc::malformed_data),
           "Seconds beyond 59 are malformed platform data");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("+0a00"),
           "A non-digit character is malformed platform data");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("+08000"),
           "An over-long rendering is malformed platform data");
}

void test_text_boundaries() {
    const auto valid =
        syscape::detail::locale_common::validate_utf8_label(
            syscape::result<std::string>("zh_CN.UTF-8"));
    expect(valid && *valid == "zh_CN.UTF-8",
           "A valid UTF-8 label passes the public boundary");

    const auto empty =
        syscape::detail::locale_common::validate_utf8_label(
            syscape::result<std::string>(""));
    expect(!empty &&
               empty.error() == syscape::make_error_code(
                                    syscape::errc::malformed_data),
           "An empty label is rejected as malformed platform data");

    const auto invalid =
        syscape::detail::locale_common::validate_utf8_label(
            syscape::result<std::string>(std::string(1U, static_cast<char>(0xff))));
    expect(!invalid &&
               invalid.error() == syscape::errc::invalid_encoding,
           "A non-UTF-8 label must fail at the public boundary");

    const auto zero_offset =
        syscape::detail::locale_common::validate_utc_offset_seconds(
            syscape::result<std::int32_t>(0));
    expect(zero_offset && *zero_offset == 0,
           "Zero is a valid offset, never an error sentinel");

    const auto extreme_offset =
        syscape::detail::locale_common::validate_utc_offset_seconds(
            syscape::result<std::int32_t>(86399));
    expect(extreme_offset && *extreme_offset == 86399,
           "The largest representable day fraction is a valid offset");

    const auto day_offset =
        syscape::detail::locale_common::validate_utc_offset_seconds(
            syscape::result<std::int32_t>(86400));
    expect(!day_offset &&
               day_offset.error() == syscape::make_error_code(
                                         syscape::errc::malformed_data),
           "A full-day offset is malformed platform data");

    const auto negative_day_offset =
        syscape::detail::locale_common::validate_utc_offset_seconds(
            syscape::result<std::int32_t>(-86400));
    expect(!negative_day_offset &&
               negative_day_offset.error() == syscape::errc::malformed_data,
           "A negative full-day offset is malformed platform data");
}

/// Independently parses a "%z" rendering with the C standard-library scan
/// functions so that the backend parser is cross-checked rather than
/// re-executed.
bool reference_offset(const std::tm& local, long& seconds) {
    char formatted[16];
    const std::size_t length =
        std::strftime(formatted, sizeof(formatted), "%z", &local);
    if (length != 5U && length != 7U) { return false; }
    if (formatted[0] != '+' && formatted[0] != '-') { return false; }
    if (!std::isdigit(static_cast<unsigned char>(formatted[1])) ||
        !std::isdigit(static_cast<unsigned char>(formatted[2])) ||
        !std::isdigit(static_cast<unsigned char>(formatted[3])) ||
        !std::isdigit(static_cast<unsigned char>(formatted[4])) ||
        (length == 7U &&
         (!std::isdigit(static_cast<unsigned char>(formatted[5])) ||
          !std::isdigit(static_cast<unsigned char>(formatted[6]))))) {
        return false;
    }

    int hours = 0;
    int minutes = 0;
    int offset_seconds = 0;
    const int fields = std::sscanf(formatted + 1, "%2d%2d%2d", &hours,
                                   &minutes, &offset_seconds);
    if (fields != (length == 7U ? 3 : 2) || hours > 23 || minutes > 59 ||
        offset_seconds > 59) {
        return false;
    }
    seconds = (hours * 3600L + minutes * 60L + offset_seconds) *
              (formatted[0] == '-' ? -1L : 1L);
    return true;
}

void test_runtime_queries() {
    const char* reference_locale = ::setlocale(LC_ALL, nullptr);
    const auto locale = syscape::locale::current_locale();
    expect(locale && reference_locale != nullptr && *locale == reference_locale,
           "Linux must report the C runtime's locale string verbatim");

    const char* reference_encoding = ::nl_langinfo(CODESET);
    const auto encoding = syscape::locale::text_encoding();
    expect(encoding && reference_encoding != nullptr &&
               *encoding == reference_encoding,
           "Linux must report the LC_CTYPE codeset name verbatim");

    ::tzset();
    const std::time_t now = ::time(nullptr);
    std::tm local {};
    expect(::localtime_r(&now, &local) != nullptr,
           "The localtime_r reference lookup must not fail natively");

    long reference_seconds = 0;
    const auto offset = syscape::locale::utc_offset_seconds();
    if (reference_offset(local, reference_seconds)) {
        expect(offset && *offset == static_cast<std::int32_t>(reference_seconds),
               "Linux must report the current UTC offset from the zone rules");
    } else {
        expect(!offset &&
                   offset.error() ==
                       syscape::make_error_code(syscape::errc::not_found),
               "An indeterminable reference zone must report not_found");
    }
}

} // namespace

int main() {
    test_offset_parsing();
    test_text_boundaries();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
