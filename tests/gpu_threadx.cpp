#include <iostream>

#include <syscape/gpu.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_gpu_queries() {
    const auto devs = syscape::gpu::devices();
    expect(!devs && devs.error() == syscape::errc::not_supported,
           "devices must report not_supported on ThreadX");

    const auto count = syscape::gpu::device_count();
    expect(!count && count.error() == syscape::errc::not_supported,
           "device_count must report not_supported on ThreadX");

    const auto primary = syscape::gpu::primary_device();
    expect(!primary && primary.error() == syscape::errc::not_supported,
           "primary_device must report not_supported on ThreadX");
}

} // namespace

int main() {
    test_gpu_queries();
    return failures == 0 ? 0 : 1;
}
