#include <cstdlib>
#include <iostream>
#include <string>

#include <syscape/locale.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_locale_queries() {
    const auto loc = syscape::locale::current_locale();
    expect(loc && !loc->empty(), "current locale query must succeed");

    const auto enc = syscape::locale::text_encoding();
    expect(enc && !enc->empty(), "text encoding query must succeed");

    const auto offset = syscape::locale::utc_offset_seconds();
    expect(offset.has_value(), "utc offset query must succeed");

    const auto tz = syscape::locale::time_zone_identifier();
    expect(tz.has_value() || tz.error() == syscape::errc::not_found ||
               tz.error() == syscape::errc::not_supported,
           "time zone query must succeed or report a documented error");

    const auto langs = syscape::locale::preferred_languages();
    expect(langs.error() == syscape::errc::not_supported,
           "preferred languages must report not_supported on Redox OS");

    const auto country = syscape::locale::country_region_code();
    expect(country.error() == syscape::errc::not_supported,
           "country region code must report not_supported on Redox OS");
}

void test_timezone_overrides() {
    const char* old_tz = std::getenv("TZ");
    std::string saved_tz;
    const bool had_tz = (old_tz != nullptr);
    if (had_tz) {
        saved_tz = old_tz;
    }

    // POSIX rule string must return not_found
    ::setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
    const auto rule_res = syscape::locale::time_zone_identifier();
    expect(!rule_res && rule_res.error() == syscape::errc::not_found,
           "POSIX rule string must report not_found");

    // Colon-prefixed UTC
    ::setenv("TZ", ":UTC", 1);
    const auto utc_colon = syscape::locale::time_zone_identifier();
    expect(utc_colon && *utc_colon == "UTC", ":UTC must resolve to UTC");

    // Plain UTC
    ::setenv("TZ", "UTC", 1);
    const auto utc_plain = syscape::locale::time_zone_identifier();
    expect(utc_plain && *utc_plain == "UTC", "UTC must resolve to UTC");

    if (had_tz) {
        ::setenv("TZ", saved_tz.c_str(), 1);
    } else {
        ::unsetenv("TZ");
    }
}

} // namespace

int main() {
    test_locale_queries();
    test_timezone_overrides();
    return failures == 0 ? 0 : 1;
}
