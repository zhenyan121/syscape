#include <iostream>
#include <net/if.h>
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

    const unsigned int lo_idx = ::if_nametoindex("lo");
    if (lo_idx > 0) {
        auto addr2 = syscape::detail::network_backend::parse_resolver_address(
            "fe80::1%lo");
        expect(addr2.has_value() && addr2->scope_id == lo_idx,
               "named interface scope must resolve");
    }

    auto addr_bad = syscape::detail::network_backend::parse_resolver_address(
        "fe80::1%nonexistent_if_xyz_99");
    expect(!addr_bad && addr_bad.error() == syscape::errc::malformed_data,
           "nonexistent named scope must fail with malformed_data");
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
           "not_supported");

    const auto dns = syscape::network::dns();
    expect(dns.has_value() || dns.error() == syscape::errc::not_found ||
               dns.error() == syscape::errc::not_supported,
           "DNS configuration query must succeed, report not_found, or report "
           "not_supported");

    const auto routes = syscape::network::routes();
    expect(routes.error() == syscape::errc::not_supported,
           "routes query must report not_supported on GNU/Hurd");

    const auto stats = syscape::network::statistics();
    expect(stats.error() == syscape::errc::not_supported,
           "statistics query must report not_supported on GNU/Hurd");
}

} // namespace

int main() {
    test_resolver_address_parsing();
    test_network_queries();
    return failures == 0 ? 0 : 1;
}
