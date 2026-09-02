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
    const auto loc = syscape::locale::system_locale();
    expect(loc.has_value(), "system locale query must succeed");

    const auto enc = syscape::locale::preferred_encoding();
    expect(enc && !enc->empty(), "preferred encoding must be nonempty");
}

} // namespace

int main() {
    test_locale_queries();
    return failures == 0 ? 0 : 1;
}
