#include <iostream>

#include <syscape/hardware.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_hardware_queries() {
    const auto mfg = syscape::hardware::system_manufacturer();
    expect(mfg.error() == syscape::errc::not_supported,
           "system manufacturer query must report not_supported on VxWorks");

    const auto prod = syscape::hardware::system_product_name();
    expect(prod.error() == syscape::errc::not_supported,
           "system product name query must report not_supported on VxWorks");

    const auto pci = syscape::hardware::pci_devices();
    expect(pci.error() == syscape::errc::not_supported,
           "pci devices query must report not_supported on VxWorks");

    const auto usb = syscape::hardware::usb_devices();
    expect(usb.error() == syscape::errc::not_supported,
           "usb devices query must report not_supported on VxWorks");

    const auto mem = syscape::hardware::memory_devices();
    expect(mem.error() == syscape::errc::not_supported,
           "memory devices query must report not_supported on VxWorks");
}

} // namespace

int main() {
    test_hardware_queries();
    return failures == 0 ? 0 : 1;
}
