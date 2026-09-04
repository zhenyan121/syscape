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
    expect(!mfg && mfg.error() == syscape::errc::not_supported,
           "system manufacturer must report not_supported on Haiku");
    const auto pci = syscape::hardware::pci_devices();
    expect(!pci && pci.error() == syscape::errc::not_supported,
           "pci devices must report not_supported on Haiku");
}

} // namespace

int main() {
    test_hardware_queries();
    return failures == 0 ? 0 : 1;
}
