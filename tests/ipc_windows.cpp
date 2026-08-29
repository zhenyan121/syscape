#include <iostream>
#include <syscape/detail/ipc/windows.hpp>
#include <syscape/ipc.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_windows_ipc_backend() {
    const auto shm = syscape::ipc::shared_memory_segments();
    expect(!shm && shm.error() == syscape::errc::not_supported,
           "SysV/POSIX shm must report not_supported on Win32");

    const auto mq = syscape::ipc::message_queues();
    expect(!mq && mq.error() == syscape::errc::not_supported,
           "SysV/POSIX msg queues must report not_supported on Win32");

    const auto sem = syscape::ipc::semaphore_sets();
    expect(!sem && sem.error() == syscape::errc::not_supported,
           "SysV/POSIX semaphores must report not_supported on Win32");

    const auto socks = syscape::ipc::local_sockets();
    expect(!socks && socks.error() == syscape::errc::not_supported,
           "Local sockets must report not_supported on Win32");

    const auto lim = syscape::ipc::limits();
    expect(!lim && lim.error() == syscape::errc::not_supported,
           "IPC limits must report not_supported on Win32");
}

} // namespace

int main() {
    test_windows_ipc_backend();
    return failures == 0 ? 0 : 1;
}
