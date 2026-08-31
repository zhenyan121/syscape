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
    expect(mfg.has_value() || mfg.error() == syscape::errc::not_found,
           "system_manufacturer must return string or not_found");

    const auto pci = syscape::hardware::pci_devices();
    expect(pci.has_value() || pci.error() == syscape::errc::permission_denied ||
               pci.error() == syscape::errc::not_supported,
           "pci_devices query must return list, permission_denied, or "
           "not_supported");
}

} // namespace

int main() {
    test_hardware_queries();
    return failures == 0 ? 0 : 1;
}
