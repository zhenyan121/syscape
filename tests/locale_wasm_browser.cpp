#include <iostream>

#include <syscape/execution_environment.hpp>
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
    expect(syscape::target_operating_system() ==
               syscape::operating_system::browser_wasm,
           "target_operating_system must report browser_wasm");

    const auto loc = syscape::locale::current_locale();
    expect(loc.has_value() && !loc->empty(),
           "current_locale must return non-empty string");

    const auto enc = syscape::locale::text_encoding();
    expect(enc.has_value() && !enc->empty(),
           "text_encoding must return non-empty string");

    const auto offset = syscape::locale::utc_offset_seconds();
    expect(!offset && offset.error() == syscape::errc::not_supported,
           "utc_offset_seconds must report not_supported");

    const auto langs = syscape::locale::preferred_languages();
    expect(!langs && langs.error() == syscape::errc::not_supported,
           "preferred_languages must report not_supported");

    const auto tz = syscape::locale::time_zone_identifier();
    expect(!tz && tz.error() == syscape::errc::not_supported,
           "time_zone_identifier must report not_supported");
}

} // namespace

int main() {
    test_locale_queries();
    return failures == 0 ? 0 : 1;
}
