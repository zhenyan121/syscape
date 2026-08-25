#include <iostream>
#include <syscape/camera.hpp>
#include <system_error>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_macos_camera_backend() {
    const auto devs = syscape::camera::devices();
    if (devs) {
        for (const auto& dev : *devs) {
            expect(!dev.id.empty(), "Device id must not be empty");
            expect(!dev.name.empty(), "Device name must not be empty");
        }
    } else {
        expect(static_cast<bool>(devs.error()),
               "Failure must carry a nonzero error code");
    }

    const auto count = syscape::camera::device_count();
    expect(count || static_cast<bool>(count.error()),
           "device_count failure must carry an error code");

    const auto capture = syscape::camera::capture_devices();
    expect(
        !capture && capture.error() == syscape::errc::not_supported,
        "Unimplemented CMIO stream classification must report not_supported");

    const auto def = syscape::camera::default_device();
    expect(!def && def.error() == syscape::errc::not_supported,
           "macOS must not guess a default camera");
}

void test_macos_camera_helpers() {
    const auto converted = syscape::detail::camera_backend::copy_utf8_string(
        CFSTR("FaceTime HD Camera"));
    expect(converted && *converted == "FaceTime HD Camera",
           "Core Foundation text must convert to UTF-8");

    const auto null_value =
        syscape::detail::camera_backend::copy_utf8_string(nullptr);
    expect(!null_value, "Null Core Foundation text must fail");

    const auto err = syscape::detail::camera_backend::map_osstatus(
        kCMIOHardwareBadObjectError);
    expect(static_cast<bool>(err), "OSStatus error must map to error_code");

    const auto valid_count =
        syscape::detail::camera_backend::cmio_device_count_from_size(
            static_cast<UInt32>(2U * sizeof(CMIOObjectID)));
    expect(valid_count && *valid_count == 2U,
           "Aligned CMIO device storage size must parse");
    const auto malformed_count =
        syscape::detail::camera_backend::cmio_device_count_from_size(
            static_cast<UInt32>(sizeof(CMIOObjectID) + 1U));
    expect(!malformed_count &&
               malformed_count.error() == syscape::errc::malformed_data,
           "Misaligned CMIO device storage size must fail");
}

} // namespace

int main() {
    test_macos_camera_helpers();
    test_macos_camera_backend();
    return failures == 0 ? 0 : 1;
}
