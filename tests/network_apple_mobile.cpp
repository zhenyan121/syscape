#include <iostream>
#include <system_error>

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
    expect(ifaces.has_value() ||
               ifaces.error() == syscape::errc::permission_denied ||
               ifaces.error() == syscape::errc::not_supported ||
               ifaces.error() == std::errc::permission_denied ||
               ifaces.error() == std::errc::operation_not_permitted,
           "interfaces query must succeed or report expected error");

    const auto routes = syscape::network::routes();
    expect(routes.error() == syscape::errc::not_supported,
           "routes query must report not_supported");

    const auto gateways = syscape::network::default_gateways();
    expect(gateways.error() == syscape::errc::not_supported,
           "gateway query must report not_supported");

    const auto dns = syscape::network::dns();
    expect(dns.error() == syscape::errc::not_supported,
           "dns query must report not_supported");

    const auto stats = syscape::network::statistics();
    expect(stats.error() == syscape::errc::not_supported,
           "statistics query must report not_supported");

    const auto named_stats = syscape::network::statistics("lo0");
    expect(named_stats.error() == syscape::errc::not_supported,
           "named statistics query must report not_supported");

    const auto indexed_stats = syscape::network::statistics(1U);
    expect(indexed_stats.error() == syscape::errc::not_supported,
           "indexed statistics query must report not_supported");
}

} // namespace

int main() {
    test_network_queries();
    return failures == 0 ? 0 : 1;
}
