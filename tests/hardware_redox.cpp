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
           "system manufacturer must report not_supported on Redox OS");

    const auto prod = syscape::hardware::system_product_name();
    expect(prod.error() == syscape::errc::not_supported,
           "system product name must report not_supported on Redox OS");

    const auto prod_ver = syscape::hardware::system_product_version();
    expect(prod_ver.error() == syscape::errc::not_supported,
           "system product version must report not_supported on Redox OS");

    const auto uuid = syscape::hardware::hardware_uuid();
    expect(uuid.error() == syscape::errc::not_supported,
           "hardware_uuid must report not_supported on Redox OS");

    const auto pci = syscape::hardware::pci_devices();
    expect(pci.error() == syscape::errc::not_supported,
           "pci devices query must report not_supported on Redox OS");
}

} // namespace

int main() {
    test_hardware_queries();
    return failures == 0 ? 0 : 1;
}
