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
    expect(mfg.has_value() || mfg.error() == syscape::errc::not_supported ||
               mfg.error() == syscape::errc::not_found,
           "system manufacturer query must succeed or report not_supported / "
           "not_found");

    const auto prod = syscape::hardware::system_product_name();
    expect(prod.error() == syscape::errc::not_supported,
           "system product name must report not_supported on Solaris");

    const auto prod_ver = syscape::hardware::system_product_version();
    expect(!prod_ver && prod_ver.error() == syscape::errc::not_supported,
           "system product version must report not_supported on Solaris");

    const auto uuid = syscape::hardware::hardware_uuid();
    expect(!uuid && uuid.error() == syscape::errc::not_supported,
           "hardware_uuid must report not_supported on Solaris");

    const auto pci = syscape::hardware::pci_devices();
    expect(!pci && pci.error() == syscape::errc::not_supported,
           "pci devices query must report not_supported on Solaris");
}

} // namespace

int main() {
    test_hardware_queries();
    return failures == 0 ? 0 : 1;
}
