#include <cassert>
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

void test_macos_connection_backend() {
#if defined(__APPLE__) && defined(__MACH__)
    using syscape::detail::connection_backend::macos_impl::map_macos_tcp_state;
    using syscape::connection::tcp_state;

    expect(map_macos_tcp_state(TCPS_CLOSED) == tcp_state::closed, "CLOSED mapping");
    expect(map_macos_tcp_state(TCPS_LISTEN) == tcp_state::listen, "LISTEN mapping");
    expect(map_macos_tcp_state(TCPS_SYN_SENT) == tcp_state::syn_sent, "SYN_SENT mapping");
    expect(map_macos_tcp_state(TCPS_SYN_RECEIVED) == tcp_state::syn_recv, "SYN_RCV mapping");
    expect(map_macos_tcp_state(TCPS_ESTABLISHED) == tcp_state::established, "ESTAB mapping");
    expect(map_macos_tcp_state(TCPS_CLOSE_WAIT) == tcp_state::close_wait, "CLOSE_WAIT mapping");
    expect(map_macos_tcp_state(TCPS_FIN_WAIT_1) == tcp_state::fin_wait1, "FIN_WAIT1 mapping");
    expect(map_macos_tcp_state(TCPS_CLOSING) == tcp_state::closing, "CLOSING mapping");
    expect(map_macos_tcp_state(TCPS_LAST_ACK) == tcp_state::last_ack, "LAST_ACK mapping");
    expect(map_macos_tcp_state(TCPS_FIN_WAIT_2) == tcp_state::fin_wait2, "FIN_WAIT2 mapping");
    expect(map_macos_tcp_state(TCPS_TIME_WAIT) == tcp_state::time_wait, "TIME_WAIT mapping");

    const auto conns = syscape::connection::connections();
    if (conns) {
        for (const auto& c : *conns) {
            expect(c.local_endpoint.port <= 65535, "Port validity");
        }
    } else {
        expect(static_cast<bool>(conns.error()), "Failure must carry error code");
    }
#endif
}

} // namespace

int main() {
    test_macos_connection_backend();
    return failures == 0 ? 0 : 1;
}
