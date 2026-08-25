#include <cassert>
#include <syscape/printer.hpp>

int main() {
    const auto list = syscape::printer::printers();
    assert(!list);
    assert(list.error() == syscape::errc::not_supported);

    const auto count = syscape::printer::printer_count();
    assert(!count);
    assert(count.error() == syscape::errc::not_supported);

    const auto def = syscape::printer::default_printer();
    assert(!def);
    assert(def.error() == syscape::errc::not_supported);

    const auto find = syscape::printer::find_printer("test");
    assert(!find);
    assert(find.error() == syscape::errc::not_supported);

    return 0;
}
