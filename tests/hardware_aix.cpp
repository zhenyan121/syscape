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
    expect(prod.has_value() || prod.error() == syscape::errc::not_supported ||
               prod.error() == syscape::errc::not_found,
           "system product name must succeed or report error");

    const auto prod_ver = syscape::hardware::system_product_version();
    expect(prod_ver.has_value() ||
               prod_ver.error() == syscape::errc::not_supported ||
               prod_ver.error() == syscape::errc::not_found,
           "system product version must succeed or report error");

    const auto uuid = syscape::hardware::hardware_uuid();
    expect(uuid.has_value() || uuid.error() == syscape::errc::not_supported ||
               uuid.error() == syscape::errc::not_found,
           "hardware_uuid must succeed or report error");

    const auto pci = syscape::hardware::pci_devices();
    expect(pci.has_value() || pci.error() == syscape::errc::not_supported,
           "pci devices query must succeed or report not_supported");
}

} // namespace

int main() {
    test_hardware_queries();
    return failures == 0 ? 0 : 1;
}
