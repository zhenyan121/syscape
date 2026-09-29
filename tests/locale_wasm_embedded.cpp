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
               syscape::operating_system::embedded_wasm,
           "target_operating_system must report embedded_wasm");

    const auto loc = syscape::locale::current_locale();
    expect(!loc && loc.error() == syscape::errc::not_supported,
           "current_locale must report not_supported on embedded WASM");

    const auto enc = syscape::locale::text_encoding();
    expect(!enc && enc.error() == syscape::errc::not_supported,
           "text_encoding must report not_supported on embedded WASM");

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
