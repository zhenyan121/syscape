#include <clocale>
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
    expect(loc.has_value() && !loc->empty(),
           "current_locale must return non-empty string");

    const auto enc = syscape::locale::text_encoding();
    expect(enc.has_value() && !enc->empty(),
           "text_encoding must return non-empty string");

    // Verify dynamic snapshot reflects setlocale
    const char* prev = std::setlocale(LC_CTYPE, nullptr);
    const std::string saved_loc = prev != nullptr ? prev : "C";

    if (std::setlocale(LC_CTYPE, "C") != nullptr) {
        const auto c_loc = syscape::locale::current_locale();
        expect(c_loc.has_value() && *c_loc == "C",
               "current_locale under 'C' must be 'C'");
    }

    std::setlocale(LC_CTYPE, saved_loc.c_str());

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
