#include <iostream>
#include <string>
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

void test_resolver_address_parsing() {
    auto addr_ipv4 =
        syscape::detail::network_backend::parse_resolver_address("192.168.1.1");
    expect(addr_ipv4.has_value() &&
               addr_ipv4->family ==
                   syscape::detail::network_common::address_family::ipv4,
           "valid IPv4 address must parse");

    auto addr_ipv6 =
        syscape::detail::network_backend::parse_resolver_address("fe80::1");
    expect(addr_ipv6.has_value() &&
               addr_ipv6->family ==
                   syscape::detail::network_common::address_family::ipv6,
           "valid IPv6 address must parse");

    auto addr_scope =
        syscape::detail::network_backend::parse_resolver_address("fe80::1%1");
    expect(addr_scope.has_value() && addr_scope->scope_id == 1U,
           "IPv6 numeric scope zone must parse");

    auto addr_named = syscape::detail::network_backend::parse_resolver_address(
        "fe80::1%eth0");
    expect(!addr_named && addr_named.error() == syscape::errc::not_supported,
           "IPv6 named scope zone must return not_supported");

    auto addr_non_ascii =
        syscape::detail::network_backend::parse_resolver_address(
            "192.168.1.1\x80");
    expect(!addr_non_ascii &&
               addr_non_ascii.error() == syscape::errc::malformed_data,
           "non-ASCII address must fail with malformed_data");

    auto addr_control =
        syscape::detail::network_backend::parse_resolver_address(
            "192.168.1.1\n");
    expect(!addr_control &&
               addr_control.error() == syscape::errc::malformed_data,
           "control characters in address must fail with malformed_data");
}

void test_dns_parsing() {
    const char* sample = "nameserver 1.1.1.1\n"
                         "nameserver 8.8.8.8\n"
                         "search example.com local\n";
    const auto res =
        syscape::detail::network_backend::parse_dns_content(sample);
    expect(res.has_value(), "parse_dns_content must succeed on valid sample");
    if (res) {
        expect(res->servers.size() == 2U, "must parse two servers");
        expect(res->search_domains.has_value() &&
                   res->search_domains->size() == 2U,
               "must parse two search domains");
    }
}

void test_network_queries() {
    const auto ifaces = syscape::network::interfaces();
    expect(ifaces.error() == syscape::errc::not_supported,
           "interfaces query must report not_supported on MINIX");

    const auto routes = syscape::network::routes();
    expect(routes.error() == syscape::errc::not_supported,
           "routes query must report not_supported on MINIX");

    const auto gateways = syscape::network::default_gateways();
    expect(gateways.error() == syscape::errc::not_supported,
           "default_gateways query must report not_supported on MINIX");

    const auto dns = syscape::network::dns();
    expect(dns.has_value() || dns.error() == syscape::errc::not_found,
           "dns query must succeed or report not_found");

    const auto stats = syscape::network::statistics();
    expect(stats.error() == syscape::errc::not_supported,
           "statistics query must report not_supported on MINIX");
}

} // namespace

int main() {
    test_resolver_address_parsing();
    test_dns_parsing();
    test_network_queries();
    return failures == 0 ? 0 : 1;
}
