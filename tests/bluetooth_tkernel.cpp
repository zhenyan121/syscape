#include <iostream>

#include <syscape/bluetooth.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_bluetooth_queries() {
    const auto a = syscape::bluetooth::adapters();
    expect(!a && a.error() == syscape::errc::not_supported,
           "adapters must report not_supported on T-Kernel");

    const auto count = syscape::bluetooth::adapter_count();
    expect(!count && count.error() == syscape::errc::not_supported,
           "adapter_count must report not_supported on T-Kernel");

    const auto def = syscape::bluetooth::default_adapter();
    expect(!def && def.error() == syscape::errc::not_supported,
           "default_adapter must report not_supported on T-Kernel");

    const auto paired = syscape::bluetooth::paired_devices();
    expect(!paired && paired.error() == syscape::errc::not_supported,
           "paired_devices must report not_supported on T-Kernel");

    const auto conn = syscape::bluetooth::connected_devices();
    expect(!conn && conn.error() == syscape::errc::not_supported,
           "connected_devices must report not_supported on T-Kernel");
}

} // namespace

int main() {
    test_bluetooth_queries();
    return failures == 0 ? 0 : 1;
}
