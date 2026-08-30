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

void test_windows_connection_backend() {
#if defined(_WIN32)
    using syscape::detail::connection_backend::windows_impl::map_windows_tcp_state;
    using syscape::detail::connection_backend::windows_impl::is_unsupported_table_error;
    using syscape::connection::tcp_state;

    expect(map_windows_tcp_state(MIB_TCP_STATE_CLOSED) == tcp_state::closed, "CLOSED mapping");
    expect(map_windows_tcp_state(MIB_TCP_STATE_LISTEN) == tcp_state::listen, "LISTEN mapping");
    expect(map_windows_tcp_state(MIB_TCP_STATE_SYN_SENT) == tcp_state::syn_sent, "SYN_SENT mapping");
    expect(map_windows_tcp_state(MIB_TCP_STATE_SYN_RCVD) == tcp_state::syn_recv, "SYN_RCVD mapping");
    expect(map_windows_tcp_state(MIB_TCP_STATE_ESTAB) == tcp_state::established, "ESTAB mapping");
    expect(map_windows_tcp_state(MIB_TCP_STATE_FIN_WAIT1) == tcp_state::fin_wait1, "FIN_WAIT1 mapping");
    expect(map_windows_tcp_state(MIB_TCP_STATE_FIN_WAIT2) == tcp_state::fin_wait2, "FIN_WAIT2 mapping");
    expect(map_windows_tcp_state(MIB_TCP_STATE_CLOSE_WAIT) == tcp_state::close_wait, "CLOSE_WAIT mapping");
    expect(map_windows_tcp_state(MIB_TCP_STATE_CLOSING) == tcp_state::closing, "CLOSING mapping");
    expect(map_windows_tcp_state(MIB_TCP_STATE_LAST_ACK) == tcp_state::last_ack, "LAST_ACK mapping");
    expect(map_windows_tcp_state(MIB_TCP_STATE_TIME_WAIT) == tcp_state::time_wait, "TIME_WAIT mapping");

    int calls = 0;
    const auto resized =
        syscape::detail::connection_backend::windows_impl::query_dynamic_table(
            [&calls](PVOID buffer, PDWORD size) -> DWORD {
                ++calls;
                if (buffer == nullptr) {
                    *size = 4;
                    return ERROR_INSUFFICIENT_BUFFER;
                }
                if (calls == 2) {
                    *size = 8;
                    return ERROR_INSUFFICIENT_BUFFER;
                }
                *size = 4;
                return NO_ERROR;
            });
    expect(resized.has_value(), "buffer-size race must be retried");
    expect(calls == 3, "buffer-size race retry count");

    const auto denied =
        syscape::detail::connection_backend::windows_impl::query_dynamic_table(
            [](PVOID, PDWORD) -> DWORD { return ERROR_ACCESS_DENIED; });
    expect(!denied, "native table errors must be preserved");
    expect(denied.error() ==
               std::error_code(ERROR_ACCESS_DENIED, std::system_category()),
           "native table error category");
    expect(is_unsupported_table_error(
               std::error_code(ERROR_NOT_SUPPORTED, std::system_category())),
           "unsupported table classification");
    expect(!is_unsupported_table_error(denied.error()),
           "non-unsupported table errors remain fatal");

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
    test_windows_connection_backend();
    return failures == 0 ? 0 : 1;
}
