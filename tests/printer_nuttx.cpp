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
    const auto prts = syscape::printer::printers();
    expect(prts.error() == syscape::errc::not_supported,
           "printers query must report not_supported on NuttX");

    const auto count = syscape::printer::printer_count();
    expect(count.error() == syscape::errc::not_supported,
           "printer_count must report not_supported on NuttX");
}

} // namespace

int main() {
    test_printer_queries();
    return failures == 0 ? 0 : 1;
}
