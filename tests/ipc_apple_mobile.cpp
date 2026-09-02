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
    expect(shm.has_value() || shm.error() == syscape::errc::not_supported ||
               shm.error() == syscape::errc::permission_denied,
           "shared memory query must succeed or report expected error");

    const auto mq = syscape::ipc::message_queues();
    expect(mq.has_value() || mq.error() == syscape::errc::not_supported ||
               mq.error() == syscape::errc::permission_denied,
           "message queues query must succeed or report expected error");
}

} // namespace

int main() {
    test_ipc_queries();
    return failures == 0 ? 0 : 1;
}
