#include <cassert>
#include <syscape/display.hpp>

int main() {
    const auto disps = syscape::display::displays();
    assert(!disps);
    assert(disps.error() == syscape::errc::not_supported);

    const auto count = syscape::display::display_count();
    assert(!count);
    assert(count.error() == syscape::errc::not_supported);

    const auto primary = syscape::display::primary_display();
    assert(!primary);
    assert(primary.error() == syscape::errc::not_supported);

    return 0;
}
