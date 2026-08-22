#include <cctype>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <system_error>
#include <time.h>

#include <syscape/detail/locale/common.hpp>
#include <syscape/detail/locale/macos.hpp>
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
    const auto plus_nine_thirty =
        syscape::detail::locale_backend::parse_gmt_offset_text("+0930");
    expect(plus_nine_thirty && *plus_nine_thirty == 34200,
           "A +0930 rendering must parse to 34200 seconds");

    const auto zero =
        syscape::detail::locale_backend::parse_gmt_offset_text("-0000");
    expect(zero && *zero == 0,
           "A negative-zero rendering is a valid zero offset");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("+2400"),
           "Hours beyond 23 are malformed platform data");
}

void test_text_boundaries() {
    const auto empty =
        syscape::detail::locale_common::validate_utf8_label(
            syscape::result<std::string>(""));
    expect(!empty &&
               empty.error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "An empty label is rejected as malformed platform data");

    const auto invalid =
        syscape::detail::locale_common::validate_utf8_label(
            syscape::result<std::string>(std::string("/tmp/\xffx")));
    expect(!invalid &&
               invalid.error() == syscape::errc::invalid_encoding,
           "A non-UTF-8 label must fail at the public boundary");

    const auto day_offset =
        syscape::detail::locale_common::validate_utc_offset_seconds(
            syscape::result<std::int32_t>(90000));
    expect(!day_offset &&
               day_offset.error() == syscape::errc::malformed_data,
           "An out-of-day-range offset is malformed platform data");
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
    const auto locale = syscape::locale::current_locale();
    expect(locale && !locale->empty() &&
               syscape::detail::is_valid_utf8(*locale),
           "macOS must report the C runtime's locale string verbatim");

    const auto encoding = syscape::locale::text_encoding();
    expect(encoding && !encoding->empty() &&
               syscape::detail::is_valid_utf8(*encoding),
           "macOS must report the LC_CTYPE codeset name verbatim");

    ::tzset();
    const std::time_t now = ::time(nullptr);
    std::tm local {};
    expect(::localtime_r(&now, &local) != nullptr,
           "The localtime_r reference lookup must not fail natively");

    long reference_seconds = 0;
    const auto offset = syscape::locale::utc_offset_seconds();
    if (reference_offset(local, reference_seconds)) {
        expect(offset &&
                   *offset == static_cast<std::int32_t>(reference_seconds),
               "macOS must report the current UTC offset from the zone "
               "rules");
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
