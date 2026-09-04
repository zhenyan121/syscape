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
    expect(ifaces.has_value() || ifaces.error() == syscape::errc::not_supported,
           "interfaces query must succeed or report not_supported");
    const auto dns = syscape::network::dns();
    expect(dns.has_value() || dns.error() == syscape::errc::not_found ||
               dns.error() == syscape::errc::not_supported,
           "dns query must succeed or report expected error");
    const auto r = syscape::network::routes();
    expect(!r && r.error() == syscape::errc::not_supported,
           "routes query must report not_supported on Haiku");
}

} // namespace

int main() {
    test_network_queries();
    return failures == 0 ? 0 : 1;
}
