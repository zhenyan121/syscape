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
    const auto ptrs = syscape::printer::printers();
    expect(!ptrs && ptrs.error() == syscape::errc::not_supported,
           "printers query must report not_supported on AIX");

    const auto count = syscape::printer::printer_count();
    expect(!count && count.error() == syscape::errc::not_supported,
           "printer_count must report not_supported on AIX");
}

} // namespace

int main() {
    test_printer_queries();
    return failures == 0 ? 0 : 1;
}
