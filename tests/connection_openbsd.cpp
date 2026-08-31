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
    const auto conn = syscape::connection::connections();
    expect(conn.has_value() || conn.error() == syscape::errc::not_supported,
           "connections must return list or not_supported");

    const auto tcp = syscape::connection::tcp_connections();
    expect(tcp.has_value() || tcp.error() == syscape::errc::not_supported,
           "tcp connections must return list or not_supported");

    const auto udp = syscape::connection::udp_endpoints();
    expect(udp.has_value() || udp.error() == syscape::errc::not_supported,
           "udp endpoints must return list or not_supported");

    const auto listen = syscape::connection::listening_endpoints();
    expect(listen.has_value() || listen.error() == syscape::errc::not_supported,
           "listening endpoints must return list or not_supported");

    const auto by_pid = syscape::connection::find_connections_by_process(1);
    expect(by_pid.has_value() || by_pid.error() == syscape::errc::not_supported,
           "find_connections_by_process must return list or not_supported");
}

} // namespace

int main() {
    test_connection_queries();
    return failures == 0 ? 0 : 1;
}
