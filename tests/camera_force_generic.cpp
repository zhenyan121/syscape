#include <cassert>
#include <syscape/camera.hpp>

int main() {
    const auto devs = syscape::camera::devices();
    assert(!devs);
    assert(devs.error() == syscape::errc::not_supported);

    const auto count = syscape::camera::device_count();
    assert(!count);
    assert(count.error() == syscape::errc::not_supported);

    const auto capture = syscape::camera::capture_devices();
    assert(!capture);
    assert(capture.error() == syscape::errc::not_supported);

    const auto def = syscape::camera::default_device();
    assert(!def);
    assert(def.error() == syscape::errc::not_supported);

    return 0;
}
