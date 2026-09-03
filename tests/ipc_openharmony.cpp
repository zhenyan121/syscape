#include <iostream>

#include <syscape/ipc.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_ipc_queries() {
    const auto lim = syscape::ipc::limits();
    expect(lim.has_value() || lim.error() == syscape::errc::not_supported,
           "IPC system limits query must succeed or report not_supported");

    const auto sockets = syscape::ipc::local_sockets();
    expect(!sockets && sockets.error() == syscape::errc::not_supported,
           "local sockets must report not_supported on OpenHarmony");
}

} // namespace

int main() {
    test_ipc_queries();
    return failures == 0 ? 0 : 1;
}
