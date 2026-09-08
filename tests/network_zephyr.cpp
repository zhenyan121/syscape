#include <iostream>

#include <syscape/network.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_network_queries() {
    const auto ifaces = syscape::network::interfaces();
    expect(!ifaces && ifaces.error() == syscape::errc::not_supported,
           "interfaces query must report not_supported on Zephyr");

    const auto routes = syscape::network::routes();
    expect(!routes && routes.error() == syscape::errc::not_supported,
           "routes query must report not_supported on Zephyr");

    const auto gateways = syscape::network::default_gateways();
    expect(!gateways && gateways.error() == syscape::errc::not_supported,
           "default_gateways query must report not_supported on Zephyr");

    const auto dns = syscape::network::dns();
    expect(!dns && dns.error() == syscape::errc::not_supported,
           "dns query must report not_supported on Zephyr");

    const auto stats = syscape::network::statistics();
    expect(!stats && stats.error() == syscape::errc::not_supported,
           "statistics query must report not_supported on Zephyr");
}

} // namespace

int main() {
    test_network_queries();
    return failures == 0 ? 0 : 1;
}
