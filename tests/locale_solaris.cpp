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
    const auto enc = syscape::locale::text_encoding();
    expect(enc && !enc->empty(), "text encoding query must succeed");

    const auto offset = syscape::locale::utc_offset_seconds();
    expect(offset.has_value(), "utc offset query must succeed");

    const auto tz = syscape::locale::time_zone_identifier();
    expect(tz.has_value() || tz.error() == syscape::errc::not_found,
           "time zone query must succeed or report not_found");
}

} // namespace

int main() {
    test_locale_queries();
    return failures == 0 ? 0 : 1;
}
