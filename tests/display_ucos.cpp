#include <iostream>

#include <syscape/display.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_display_queries() {
    const auto d = syscape::display::displays();
    expect(!d && d.error() == syscape::errc::not_supported,
           "displays must report not_supported on uC/OS");

    const auto count = syscape::display::display_count();
    expect(!count && count.error() == syscape::errc::not_supported,
           "display_count must report not_supported on uC/OS");

    const auto primary = syscape::display::primary_display();
    expect(!primary && primary.error() == syscape::errc::not_supported,
           "primary_display must report not_supported on uC/OS");
}

} // namespace

int main() {
    test_display_queries();
    return failures == 0 ? 0 : 1;
}
