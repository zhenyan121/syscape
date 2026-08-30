#include <iostream>

#include <syscape/printer.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_printer_queries() {
    const auto prns = syscape::printer::printers();
    expect(
        prns.has_value() || prns.error() == syscape::errc::not_supported ||
            prns.error() == syscape::errc::permission_denied,
        "printers query must return list, not_supported, or permission_denied");
}

} // namespace

int main() {
    test_printer_queries();
    return failures == 0 ? 0 : 1;
}
