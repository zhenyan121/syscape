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
           "ipc limits must return struct or not_supported");

    const auto shm = syscape::ipc::shared_memory_segments();
    expect(shm.has_value() || shm.error() == syscape::errc::not_supported,
           "shm query must return list or not_supported");

    const auto sem = syscape::ipc::semaphore_sets();
    expect(sem.has_value() || sem.error() == syscape::errc::not_supported,
           "sem query must return list or not_supported");

    const auto msg = syscape::ipc::message_queues();
    expect(msg.has_value() || msg.error() == syscape::errc::not_supported,
           "msg query must return list or not_supported");

    const auto sock = syscape::ipc::local_sockets();
    expect(sock.has_value() || sock.error() == syscape::errc::not_supported,
           "sockets query must return list or not_supported");
}

} // namespace

int main() {
    test_ipc_queries();
    return failures == 0 ? 0 : 1;
}
