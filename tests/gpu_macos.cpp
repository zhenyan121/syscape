#include <iostream>
#include <string_view>

#include <syscape/gpu.hpp>
#include <syscape/detail/gpu/common.hpp>
#include <syscape/detail/utf8.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_macos_vendor_classification() {
    namespace common = syscape::detail::gpu_common;
    using syscape::gpu::gpu_vendor;

    expect(common::classify_vendor_name("Apple M1 Max") == gpu_vendor::apple,
           "Apple M1 Max must classify as Apple");
    expect(common::classify_vendor_name("Apple M2 Pro GPU") == gpu_vendor::apple,
           "Apple M2 Pro GPU must classify as Apple");
    expect(common::classify_vendor_name("AMD Radeon Pro 5500M") == gpu_vendor::amd,
           "AMD Radeon Pro must classify as AMD");
    expect(common::classify_vendor_name("Intel Iris Plus Graphics") == gpu_vendor::intel,
           "Intel Iris Plus must classify as Intel");
    expect(common::classify_vendor_name("Generic Framebuffer") == gpu_vendor::other,
           "Generic Framebuffer must classify as other, not Apple");
    expect(common::classify_vendor_name("") == gpu_vendor::unknown,
           "Empty string must classify as unknown");
}

void test_macos_utf8_validation() {
    // Valid UTF-8 string
    const std::string valid = "Apple M3 GPU";
    expect(syscape::detail::is_valid_utf8(valid),
           "Valid model name must pass UTF-8 validation");

    // Invalid UTF-8 sequence
    const std::string invalid = "\xFF\xFE\xFD";
    expect(!syscape::detail::is_valid_utf8(invalid),
           "Invalid UTF-8 byte sequence must fail validation");
}

void test_macos_live_query() {
    const auto devs = syscape::gpu::devices();
    expect(devs.has_value(), "devices() must return a result");
    const auto count = syscape::gpu::device_count();
    expect(count.has_value() && *count == devs->size(),
           "device_count() must match devices() size");
}

} // namespace

int main() {
    test_macos_vendor_classification();
    test_macos_utf8_validation();
    test_macos_live_query();
    return failures == 0 ? 0 : 1;
}
