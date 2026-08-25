#include <iostream>
#include <limits>
#include <string_view>
#include <syscape/camera.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_windows_camera_backend() {
    const auto devs = syscape::camera::devices();
    if (devs) {
        for (const auto& dev : *devs) {
            expect(!dev.name.empty(), "Device name must not be empty");
            expect(dev.capabilities.has_value() &&
                       dev.capabilities->has_video_capture.value_or(false),
                   "Camera-interface devices must report capture capability");
        }
    } else {
        expect(static_cast<bool>(devs.error()),
               "Failure must carry a nonzero error code");
    }

    const auto count = syscape::camera::device_count();
    expect(count || static_cast<bool>(count.error()),
           "device_count failure must carry an error code");

    const auto capture = syscape::camera::capture_devices();
    if (capture && devs) {
        expect(capture->size() <= devs->size(),
               "Capture devices must not exceed total devices");
    }

    const auto def = syscape::camera::default_device();
    expect(!def && def.error() == syscape::errc::not_supported,
           "Windows must not guess a default camera");
}

void test_windows_camera_helpers() {
    using syscape::detail::camera_backend::parse_windows_hardware_id;
    const auto usb_id =
        parse_windows_hardware_id("USB\\VID_5986&PID_2175&REV_0003");
    expect(usb_id && usb_id->has_value(), "USB hardware ID must parse");
    if (usb_id && usb_id->has_value()) {
        expect((**usb_id).vendor_id == 0x5986U, "Vendor ID must match 0x5986");
        expect((**usb_id).product_id == 0x2175U,
               "Product ID must match 0x2175");
        expect((**usb_id).revision == 0x0003U, "Revision must match 0x0003");
    }

    const auto lowercase_id =
        parse_windows_hardware_id("usb\\vid_5986&pid_2175");
    expect(lowercase_id && lowercase_id->has_value(),
           "Lowercase hardware ID must parse");
    if (lowercase_id && lowercase_id->has_value()) {
        expect(!(**lowercase_id).revision.has_value(),
               "Absent revision must remain absent");
    }

    const auto malformed_id =
        parse_windows_hardware_id("USB\\VID_ZZZZ&PID_2175");
    expect(!malformed_id &&
               malformed_id.error() == syscape::errc::malformed_data,
           "Malformed hardware IDs must not be partially accepted");

    expect(syscape::detail::camera_backend::windows_error(ERROR_SUCCESS) ==
               syscape::errc::io_error,
           "A failed Win32 call must never produce a zero failure code");
    const DWORD unmappable_error =
        static_cast<DWORD>((std::numeric_limits<int>::max)()) + 1U;
    expect(syscape::detail::camera_backend::windows_error(unmappable_error) ==
               syscape::errc::io_error,
           "Win32 errors outside error_code's range must not be truncated");

    const wchar_t invalid_utf16[] = {static_cast<wchar_t>(0xD800U)};
    const auto converted = syscape::detail::camera_backend::wide_to_utf8(
        std::wstring_view(invalid_utf16, 1U));
    expect(!converted, "Invalid UTF-16 must fail conversion");

    const auto interface_available =
        syscape::detail::camera_backend::camera_interface_is_available();
    expect(interface_available ||
               static_cast<bool>(interface_available.error()),
           "Windows version lookup must return a value or an error");
    expect(!syscape::detail::camera_backend::
               camera_interface_is_available_for_version(10U, 17133U),
           "Pre-1803 Windows 10 must not use the camera interface");
    expect(syscape::detail::camera_backend::
               camera_interface_is_available_for_version(10U, 17134U),
           "Windows 10 version 1803 must support the camera interface");
}

} // namespace

int main() {
    test_windows_camera_helpers();
    test_windows_camera_backend();
    return failures == 0 ? 0 : 1;
}
