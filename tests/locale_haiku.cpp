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
    expect(loc && !loc->empty(), "current locale must be nonempty");
    const auto enc = syscape::locale::text_encoding();
    expect(enc && !enc->empty(), "text encoding must be nonempty");
    const auto offset = syscape::locale::utc_offset_seconds();
    expect(offset.has_value(), "utc offset seconds query must succeed");
}

} // namespace

int main() {
    test_locale_queries();
    return failures == 0 ? 0 : 1;
}
