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
    if (!ifaces) {
        std::cerr << "FAIL: interface enumeration failed on OpenBSD: "
                  << ifaces.error().category().name() << ':'
                  << ifaces.error().value() << ' ' << ifaces.error().message()
                  << '\n';
        ++failures;
        return;
    }
    expect(!ifaces->empty(), "interfaces must not be empty on OpenBSD");
}

} // namespace

int main() {
    test_network_queries();
    return failures == 0 ? 0 : 1;
}
