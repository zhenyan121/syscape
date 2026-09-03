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
    const auto list = syscape::connection::connections();
    expect(list.has_value() ||
               list.error() == syscape::errc::permission_denied ||
               list.error() == syscape::errc::not_supported,
           "connections query must succeed, report permission_denied, or "
           "report not_supported");
}

} // namespace

int main() {
    test_connection_queries();
    return failures == 0 ? 0 : 1;
}
