#include <cassert>
#include <syscape/gpu.hpp>

int main() {
    const auto devs = syscape::gpu::devices();
    assert(!devs);
    assert(devs.error() == syscape::errc::not_supported);

    const auto count = syscape::gpu::device_count();
    assert(!count);
    assert(count.error() == syscape::errc::not_supported);

    const auto primary = syscape::gpu::primary_device();
    assert(!primary);
    assert(primary.error() == syscape::errc::not_supported);

    return 0;
}
