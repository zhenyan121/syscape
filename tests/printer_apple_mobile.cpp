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
    expect(list.has_value() || list.error() == syscape::errc::not_supported ||
               list.error() == syscape::errc::permission_denied,
           "printers query must succeed or report expected error");
}

} // namespace

int main() {
    test_printer_queries();
    return failures == 0 ? 0 : 1;
}
