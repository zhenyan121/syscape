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
    auto addr1 =
        syscape::detail::network_backend::parse_resolver_address("fe80::1%1");
    expect(addr1.has_value() && addr1->scope_id == 1U,
           "numeric scope must parse");

    auto addr_stub = syscape::detail::network_backend::parse_resolver_address(
        "fe80::1%stub");
    expect(!addr_stub && addr_stub.error() == syscape::errc::not_supported,
           "%stub named scope must fail with not_supported");

    auto addr_lo =
        syscape::detail::network_backend::parse_resolver_address("fe80::1%lo");
    expect(!addr_lo && addr_lo.error() == syscape::errc::not_supported,
           "%lo named scope must fail with not_supported");

    auto addr_eth0 = syscape::detail::network_backend::parse_resolver_address(
        "fe80::1%eth0");
    expect(!addr_eth0 && addr_eth0.error() == syscape::errc::not_supported,
           "%eth0 named scope must fail with not_supported");

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

void test_redox_dns_format() {
    const auto res = syscape::detail::network_backend::parse_dns_content(
        "9.9.9.9\n208.67.222.222\n# comment\n", false);
    expect(res.has_value(), "Redox /etc/net/dns format must parse");
    if (res) {
        expect(res->servers.size() == 2U, "must parse two DNS servers");
        if (res->servers.size() >= 2U) {
            expect(res->servers[0].address.family ==
                       syscape::detail::network_common::address_family::ipv4,
                   "first server must be IPv4");
            expect(res->servers[0].address.value[0] == 9 &&
                       res->servers[0].address.value[1] == 9 &&
                       res->servers[0].address.value[2] == 9 &&
                       res->servers[0].address.value[3] == 9,
                   "first server IP must match 9.9.9.9");
            expect(res->servers[1].address.value[0] == 208 &&
                       res->servers[1].address.value[1] == 67 &&
                       res->servers[1].address.value[2] == 222 &&
                       res->servers[1].address.value[3] == 222,
                   "second server IP must match 208.67.222.222");
        }
    }

    const auto bad = syscape::detail::network_backend::parse_dns_content(
        "not_an_ip_address", false);
    expect(!bad && bad.error() == syscape::errc::malformed_data,
           "malformed IP in /etc/net/dns must fail with malformed_data");

    if (res) {
        expect(!res->search_domains.has_value(),
               "/etc/net/dns must leave search_domains as nullopt");
    }

    const auto resolv_no_search =
        syscape::detail::network_backend::parse_dns_content(
            "nameserver 1.1.1.1\n", true);
    expect(resolv_no_search.has_value() &&
               resolv_no_search->search_domains.has_value() &&
               resolv_no_search->search_domains->empty(),
           "resolv.conf without search must have empty search_domains");

    const auto resolv = syscape::detail::network_backend::parse_dns_content(
        "nameserver 1.1.1.1\nsearch example.com\n", true);
    expect(resolv.has_value() && resolv->servers.size() == 1U,
           "resolv.conf format must parse under fallback");
    if (resolv) {
        expect(resolv->search_domains.has_value() &&
                   resolv->search_domains->size() == 1U &&
                   resolv->search_domains->front() == "example.com",
               "resolv.conf search domains must match");
    }
}

void test_network_queries() {
    const auto ifaces = syscape::network::interfaces();
    expect(ifaces.error() == syscape::errc::not_supported,
           "interfaces query must report not_supported on Redox OS");

    const auto dns = syscape::network::dns();
    expect(dns.has_value() || dns.error() == syscape::errc::not_found ||
               dns.error() == syscape::errc::not_supported,
           "DNS configuration query must succeed, report not_found, or report "
           "not_supported");

    const auto routes = syscape::network::routes();
    expect(routes.error() == syscape::errc::not_supported,
           "routes query must report not_supported on Redox OS");

    const auto stats = syscape::network::statistics();
    expect(stats.error() == syscape::errc::not_supported,
           "statistics query must report not_supported on Redox OS");
}

} // namespace

int main() {
    test_resolver_address_parsing();
    test_redox_dns_format();
    test_network_queries();
    return failures == 0 ? 0 : 1;
}
