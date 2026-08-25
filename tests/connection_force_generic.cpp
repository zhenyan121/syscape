#include <cassert>
#include <syscape/connection.hpp>

int main() {
    const auto all = syscape::connection::connections();
    assert(!all);
    assert(all.error() == syscape::errc::not_supported);

    const auto tcp = syscape::connection::tcp_connections();
    assert(!tcp);
    assert(tcp.error() == syscape::errc::not_supported);

    const auto udp = syscape::connection::udp_endpoints();
    assert(!udp);
    assert(udp.error() == syscape::errc::not_supported);

    const auto listen = syscape::connection::listening_endpoints();
    assert(!listen);
    assert(listen.error() == syscape::errc::not_supported);

    const auto by_pid = syscape::connection::find_connections_by_process(1);
    assert(!by_pid);
    assert(by_pid.error() == syscape::errc::not_supported);

    return 0;
}
