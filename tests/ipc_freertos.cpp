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
    const auto shm = syscape::ipc::shared_memory_segments();
    expect(!shm && shm.error() == syscape::errc::not_supported,
           "shared_memory_segments must report not_supported on FreeRTOS");

    const auto mq = syscape::ipc::message_queues();
    expect(!mq && mq.error() == syscape::errc::not_supported,
           "message_queues must report not_supported on FreeRTOS");

    const auto sem = syscape::ipc::semaphore_sets();
    expect(!sem && sem.error() == syscape::errc::not_supported,
           "semaphore_sets must report not_supported on FreeRTOS");

    const auto sock = syscape::ipc::local_sockets();
    expect(!sock && sock.error() == syscape::errc::not_supported,
           "local_sockets must report not_supported on FreeRTOS");

    const auto lim = syscape::ipc::limits();
    expect(!lim && lim.error() == syscape::errc::not_supported,
           "ipc limits must report not_supported on FreeRTOS");
}

} // namespace

int main() {
    test_ipc_queries();
    return failures == 0 ? 0 : 1;
}
