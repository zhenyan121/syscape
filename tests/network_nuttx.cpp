#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <system_error>
#include <netinet/in.h>

#include <syscape/network.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_resolver_address_parsing() {
    struct sockaddr_in ipv4 {};
    ipv4.sin_family = AF_INET;
    expect(::inet_pton(AF_INET, "192.0.2.1", &ipv4.sin_addr) == 1,
           "test IPv4 address must parse");
    const auto converted4 = syscape::detail::network_backend::nuttx_dns_address(
        reinterpret_cast<const struct sockaddr*>(&ipv4), sizeof(ipv4));
    expect(converted4 &&
               converted4->family ==
                   syscape::detail::network_common::address_family::ipv4,
           "NuttX DNS callback must convert IPv4 addresses");

    struct sockaddr_in6 ipv6 {};
    ipv6.sin6_family = AF_INET6;
    ipv6.sin6_scope_id = 7U;
    expect(::inet_pton(AF_INET6, "fe80::1", &ipv6.sin6_addr) == 1,
           "test IPv6 address must parse");
    const auto converted6 = syscape::detail::network_backend::nuttx_dns_address(
        reinterpret_cast<const struct sockaddr*>(&ipv6), sizeof(ipv6));
    expect(converted6 && converted6->scope_id == 7U,
           "NuttX DNS callback must preserve IPv6 scope identifiers");

    const auto truncated = syscape::detail::network_backend::nuttx_dns_address(
        reinterpret_cast<const struct sockaddr*>(&ipv6), 1U);
    expect(!truncated && truncated.error() == syscape::errc::malformed_data,
           "truncated DNS socket addresses must be rejected");
}

void test_network_queries() {
    const auto ifaces = syscape::network::interfaces();
    expect(ifaces.has_value() ||
               ifaces.error() == syscape::errc::not_supported ||
               ifaces.error() == syscape::errc::permission_denied ||
               ifaces.error() == std::errc::operation_not_permitted ||
               ifaces.error() == std::errc::permission_denied ||
               ifaces.error() == std::errc::operation_not_supported,
           "interfaces query must succeed, report permission_denied, or report "
           "not_supported on NuttX");

    const auto routes = syscape::network::routes();
    expect(routes.error() == syscape::errc::not_supported,
           "routes query must report not_supported on NuttX");

    const auto gateways = syscape::network::default_gateways();
    expect(gateways.error() == syscape::errc::not_supported,
           "default_gateways query must report not_supported on NuttX");

    const auto dns = syscape::network::dns();
    expect(dns.error() == syscape::errc::not_supported,
           "host selection without the NuttX DNS client must be unsupported");

    const auto stats = syscape::network::statistics();
    expect(stats.error() == syscape::errc::not_supported,
           "statistics query must report not_supported on NuttX");
}

} // namespace

int main() {
    test_resolver_address_parsing();
    test_network_queries();
    return failures == 0 ? 0 : 1;
}
