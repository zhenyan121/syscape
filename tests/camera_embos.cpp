#include <iostream>

#include <syscape/camera.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_camera_queries() {
    const auto d = syscape::camera::devices();
    expect(!d && d.error() == syscape::errc::not_supported,
           "devices must report not_supported on embOS");

    const auto count = syscape::camera::device_count();
    expect(!count && count.error() == syscape::errc::not_supported,
           "device_count must report not_supported on embOS");

    const auto cap = syscape::camera::capture_devices();
    expect(!cap && cap.error() == syscape::errc::not_supported,
           "capture_devices must report not_supported on embOS");

    const auto def = syscape::camera::default_device();
    expect(!def && def.error() == syscape::errc::not_supported,
           "default_device must report not_supported on embOS");
}

} // namespace

int main() {
    test_camera_queries();
    return failures == 0 ? 0 : 1;
}
