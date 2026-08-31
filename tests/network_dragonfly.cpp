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
    expect(ifaces && !ifaces->empty(),
           "network interfaces must return at least one entry");

    const auto stats = syscape::network::statistics();
    expect(stats.has_value(), "network statistics query must succeed");

    const auto dns = syscape::network::dns();
    expect(dns || dns.error() == syscape::errc::not_found,
           "DNS configuration must succeed or report not_found");
}

} // namespace

int main() {
    test_network_queries();
    return failures == 0 ? 0 : 1;
}
