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

void test_macos_printer_backend() {
    const auto list = syscape::printer::printers();
    if (list) {
        for (const auto& p : *list) {
            expect(!p.id.empty(), "Printer id must not be empty");
            expect(!p.name.empty(), "Printer name must not be empty");
        }
    } else {
        expect(static_cast<bool>(list.error()),
               "Failure must carry a nonzero error code");
    }

    const auto count = syscape::printer::printer_count();
    expect(count || static_cast<bool>(count.error()),
           "printer_count failure must carry an error code");

    const auto def = syscape::printer::default_printer();
    if (def) {
        expect(!def->name.empty(), "Default printer name must not be empty");
    } else {
        expect(static_cast<bool>(def.error()),
               "default_printer failure must carry an error code");
    }

    const auto find = syscape::printer::find_printer("__nonexistent_printer__");
    expect(!find, "Nonexistent printer lookup must fail");
    if (list) {
        expect(find.error() == syscape::errc::not_found,
               "Nonexistent printer must return not_found error");
    } else {
        expect(find.error() == list.error(),
               "Lookup must preserve enumeration failures");
    }
}

} // namespace

int main() {
    test_macos_printer_backend();
    return failures == 0 ? 0 : 1;
}
