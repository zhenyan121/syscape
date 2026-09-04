#include <iostream>

#include <syscape/connection.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_connection_queries() {
    const auto conns = syscape::connection::connections();
    expect(!conns && conns.error() == syscape::errc::not_supported,
           "connections must report not_supported on AIX");
}

} // namespace

int main() {
    test_connection_queries();
    return failures == 0 ? 0 : 1;
}
