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
    expect(ifaces.has_value() ||
               ifaces.error() == syscape::errc::not_supported ||
               ifaces.error() == syscape::errc::permission_denied ||
               ifaces.error() == std::errc::operation_not_permitted ||
               ifaces.error() == std::errc::permission_denied ||
               ifaces.error() == std::errc::operation_not_supported,
           "interfaces query must succeed, report permission_denied, "
           "or report not_supported");

    const auto dns = syscape::network::dns();
    expect(dns.has_value() || dns.error() == syscape::errc::not_found ||
               dns.error() == syscape::errc::not_supported,
           "DNS configuration query must succeed, report not_found, or report "
           "not_supported");
    if (dns) {
        for (const auto& s : dns->servers) {
            expect(s.address.family == syscape::network::address_family::ipv4 ||
                       s.address.family ==
                           syscape::network::address_family::ipv6,
                   "DNS server must have valid address family");
        }
    }

    const auto routes = syscape::network::routes();
    expect(routes.has_value() || routes.error() == syscape::errc::not_supported,
           "routes query must succeed or report not_supported");

    const auto stats = syscape::network::statistics();
    expect(stats.has_value() || stats.error() == syscape::errc::not_supported,
           "statistics query must succeed or report not_supported");
}

} // namespace

int main() {
    test_network_queries();
    return failures == 0 ? 0 : 1;
}
