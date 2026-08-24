#include <iostream>
#include <string_view>
#include <syscape/input.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_windows_input_backend() {
    const auto devs = syscape::input::devices();
    if (devs) {
        for (const auto& dev : *devs) {
            expect(!dev.name.empty(), "Device name must not be empty");
            if (dev.hardware_id) {
                expect(dev.hardware_id->bus == dev.bus,
                       "Hardware ID bus must match device bus");
            }
        }
    } else {
        expect(static_cast<bool>(devs.error()),
               "Failure must carry a nonzero error code");
    }

    const auto kbds = syscape::input::keyboards();
    if (kbds) {
        for (const auto& dev : *kbds) {
            expect(dev.type == syscape::input::device_type::keyboard,
                   "Keyboard device must have keyboard type");
        }
    }

    const auto mice = syscape::input::mice();
    if (mice) {
        for (const auto& dev : *mice) {
            expect(dev.type == syscape::input::device_type::mouse,
                   "Mouse device must have mouse type");
        }
    }

    const auto touches = syscape::input::touch_devices();
    if (touches) {
        for (const auto& dev : *touches) {
            expect(dev.type == syscape::input::device_type::touchpad ||
                   dev.type == syscape::input::device_type::touchscreen ||
                   dev.type == syscape::input::device_type::drawing_tablet,
                   "Touch device must have touch type");
        }
    }

    const auto gamepads = syscape::input::gamepads();
    if (gamepads) {
        for (const auto& dev : *gamepads) {
            expect(dev.type == syscape::input::device_type::gamepad ||
                   dev.type == syscape::input::device_type::joystick,
                   "Gamepad device must have gamepad/joystick type");
        }
    }

    const auto count = syscape::input::device_count();
    expect(count || static_cast<bool>(count.error()),
           "device_count failure must carry an error code");
}

void test_windows_input_helpers() {
    using syscape::detail::input_backend::bus_from_device_path;
    using syscape::input::bus_type;

    expect(bus_from_device_path("\\\\?\\HID#BTHENUM#DEVICE") ==
               bus_type::bluetooth,
           "Bluetooth HID path must not be classified as USB");
    expect(bus_from_device_path("\\\\?\\USB#VID_1234") == bus_type::usb,
           "USB path must be classified as USB");
    expect(bus_from_device_path("\\\\?\\HID#VID_1234") == bus_type::unknown,
           "Generic HID namespace must not imply USB transport");

    const wchar_t invalid_utf16[] = {static_cast<wchar_t>(0xD800U)};
    const auto converted = syscape::detail::input_backend::wide_to_utf8(
        std::wstring_view(invalid_utf16, 1U));
    expect(!converted, "Invalid UTF-16 must fail conversion");
}

} // namespace

int main() {
    test_windows_input_helpers();
    test_windows_input_backend();
    return failures == 0 ? 0 : 1;
}
