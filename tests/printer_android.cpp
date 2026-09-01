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
    const auto list = syscape::printer::printers();
    expect(list.has_value() || list.error() == syscape::errc::not_supported,
           "printers query must succeed or report not_supported");

    const auto count = syscape::printer::printer_count();
    expect(count.has_value() || count.error() == syscape::errc::not_supported,
           "printer count query must succeed or report not_supported");
}

} // namespace

int main() {
    test_printer_queries();
    return failures == 0 ? 0 : 1;
}
