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
               ifaces.error() == std::errc::permission_denied,
           "network interfaces query must succeed, report permission_denied, "
           "or report not_supported");

    const auto stats = syscape::network::statistics();
    expect(stats.has_value() ||
               stats.error() == syscape::errc::permission_denied ||
               stats.error() == syscape::errc::not_supported ||
               stats.error() == std::errc::operation_not_permitted,
           "network statistics query must succeed, report permission_denied, "
           "or report not_supported");
}

} // namespace

int main() {
    test_network_queries();
    return failures == 0 ? 0 : 1;
}
