#include <iostream>

#include <syscape/wifi.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_wifi_queries() {
    const auto a = syscape::wifi::adapters();
    expect(!a && a.error() == syscape::errc::not_supported,
           "adapters must report not_supported on INTEGRITY");

    const auto count = syscape::wifi::adapter_count();
    expect(!count && count.error() == syscape::errc::not_supported,
           "adapter_count must report not_supported on INTEGRITY");

    const auto def = syscape::wifi::default_adapter();
    expect(!def && def.error() == syscape::errc::not_supported,
           "default_adapter must report not_supported on INTEGRITY");

    const auto conn = syscape::wifi::current_connection();
    expect(!conn && conn.error() == syscape::errc::not_supported,
           "current_connection must report not_supported on INTEGRITY");

    const auto networks = syscape::wifi::configured_networks();
    expect(!networks && networks.error() == syscape::errc::not_supported,
           "configured_networks must report not_supported on INTEGRITY");
}

} // namespace

int main() {
    test_wifi_queries();
    return failures == 0 ? 0 : 1;
}
