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
    const auto p = syscape::printer::printers();
    expect(!p && p.error() == syscape::errc::not_supported,
           "printers query must report not_supported on Haiku");
}

} // namespace

int main() {
    test_printer_queries();
    return failures == 0 ? 0 : 1;
}
