#include <iostream>
#include <system_error>
#include <syscape/input.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_macos_input_backend() {
    const auto devs = syscape::input::devices();
    if (devs) {
        for (const auto& dev : *devs) {
            expect(!dev.id.empty(), "Device id must not be empty");
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

void test_macos_input_helpers() {
    const auto converted =
        syscape::detail::input_backend::cf_string_to_utf8(CFSTR("Input device"));
    expect(converted && *converted == "Input device",
           "Core Foundation text must convert to UTF-8");

    expect(syscape::detail::input_backend::error_from_ioreturn(
               kIOReturnNotPrivileged) == std::errc::permission_denied,
           "Privilege failures must map to permission_denied");
    expect(syscape::detail::input_backend::error_from_ioreturn(
               kIOReturnUnsupported) == std::errc::operation_not_supported,
           "Unsupported failures must map to not_supported");
    expect(syscape::detail::input_backend::error_from_ioreturn(
               kIOReturnBusy) == std::errc::resource_unavailable_try_again,
           "Busy failures must map to temporarily_unavailable");
}

} // namespace

int main() {
    test_macos_input_helpers();
    test_macos_input_backend();
    return failures == 0 ? 0 : 1;
}
