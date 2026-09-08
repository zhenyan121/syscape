#include <iostream>

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
    expect(!loc && loc.error() == syscape::errc::not_supported,
           "current locale query must report not_supported on Zephyr");

    const auto enc = syscape::locale::text_encoding();
    expect(!enc && enc.error() == syscape::errc::not_supported,
           "text encoding query must report not_supported on Zephyr");

    const auto offset = syscape::locale::utc_offset_seconds();
    expect(!offset && offset.error() == syscape::errc::not_supported,
           "utc offset query must report not_supported on Zephyr");

    const auto tz = syscape::locale::time_zone_identifier();
    expect(!tz && tz.error() == syscape::errc::not_supported,
           "time zone query must report not_supported on Zephyr");

    const auto langs = syscape::locale::preferred_languages();
    expect(!langs && langs.error() == syscape::errc::not_supported,
           "preferred languages must report not_supported on Zephyr");

    const auto country = syscape::locale::country_region_code();
    expect(!country && country.error() == syscape::errc::not_supported,
           "country region code must report not_supported on Zephyr");
}

} // namespace

int main() {
    test_locale_queries();
    return failures == 0 ? 0 : 1;
}
