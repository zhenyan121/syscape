#include <cstdlib>
#include <ctime>
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

    // Test Western hemisphere offset (e.g. EST5 = UTC-05:00, -18000 seconds)
    const char* old_tz = ::getenv("TZ");
    std::string saved_tz = old_tz != nullptr ? old_tz : "";
    const bool had_tz = (old_tz != nullptr);

    ::setenv("TZ", "EST5", 1);
    ::tzset();
    const auto est_offset = syscape::locale::utc_offset_seconds();
    expect(est_offset.has_value() && *est_offset == -5 * 3600,
           "utc offset in EST5 must be -18000 seconds");

    // Test Eastern hemisphere offset (e.g. CST-8 = UTC+08:00, 28800 seconds)
    ::setenv("TZ", "CST-8", 1);
    ::tzset();
    const auto cst_offset = syscape::locale::utc_offset_seconds();
    expect(cst_offset.has_value() && *cst_offset == 8 * 3600,
           "utc offset in CST-8 must be 28800 seconds");

    if (had_tz) {
        ::setenv("TZ", saved_tz.c_str(), 1);
    } else {
        ::unsetenv("TZ");
    }
    ::tzset();

    const auto tz = syscape::locale::time_zone_identifier();
    expect(tz.has_value() || tz.error() == syscape::errc::not_found ||
               tz.error() == syscape::errc::not_supported,
           "time zone query must succeed or report a documented error");

    const auto langs = syscape::locale::preferred_languages();
    expect(langs.error() == syscape::errc::not_supported,
           "preferred languages must report not_supported on NuttX");

    const auto country = syscape::locale::country_region_code();
    expect(country.error() == syscape::errc::not_supported,
           "country region code must report not_supported on NuttX");
}

} // namespace

int main() {
    test_locale_queries();
    return failures == 0 ? 0 : 1;
}
