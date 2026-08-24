#include <cassert>
#include <syscape/input.hpp>

int main() {
    const auto devs = syscape::input::devices();
    assert(!devs);
    assert(devs.error() == syscape::errc::not_supported);

    const auto kbds = syscape::input::keyboards();
    assert(!kbds);
    assert(kbds.error() == syscape::errc::not_supported);

    const auto mice = syscape::input::mice();
    assert(!mice);
    assert(mice.error() == syscape::errc::not_supported);

    const auto touch = syscape::input::touch_devices();
    assert(!touch);
    assert(touch.error() == syscape::errc::not_supported);

    const auto pads = syscape::input::gamepads();
    assert(!pads);
    assert(pads.error() == syscape::errc::not_supported);

    const auto count = syscape::input::device_count();
    assert(!count);
    assert(count.error() == syscape::errc::not_supported);

    return 0;
}
