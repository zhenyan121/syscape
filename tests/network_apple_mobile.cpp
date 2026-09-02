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
    expect(routes.has_value() ||
               routes.error() == syscape::errc::not_supported ||
               routes.error() == syscape::errc::permission_denied ||
               routes.error() == std::errc::permission_denied ||
               routes.error() == std::errc::operation_not_permitted,
           "routes query must succeed or report not_supported");

    const auto dns = syscape::network::dns();
    expect(dns.has_value() || dns.error() == syscape::errc::not_supported ||
               dns.error() == syscape::errc::not_found ||
               dns.error() == std::errc::permission_denied ||
               dns.error() == std::errc::operation_not_permitted,
           "dns query must succeed or report not_supported");
}

} // namespace

int main() {
    test_network_queries();
    return failures == 0 ? 0 : 1;
}
