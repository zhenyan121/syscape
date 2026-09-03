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
    expect(loc && !loc->empty(), "current locale identifier must be nonempty");

    const auto encoding = syscape::locale::text_encoding();
    expect((encoding && !encoding->empty()) ||
               encoding.error() == syscape::errc::not_supported,
           "text encoding must be nonempty or report not_supported");
}

} // namespace

int main() {
    test_locale_queries();
    return failures == 0 ? 0 : 1;
}
