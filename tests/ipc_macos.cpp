#include <cstdint>
#include <iostream>
#include <string>
#include <syscape/detail/ipc/macos.hpp>
#include <syscape/ipc.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_macos_synthetic_ipc_limits() {
    using namespace syscape::detail::ipc_backend::macos_impl;

    const auto lim = parse_macos_sysv_ipc_limits(
        4194304ULL,
        1024ULL,
        32ULL,
        2048ULL,
        4096ULL,
        16ULL,
        256ULL,
        32U);

    expect(lim.max_shared_memory_segment_bytes.has_value() &&
           *lim.max_shared_memory_segment_bytes == 4194304ULL,
           "shmmax preserved");
    expect(lim.max_total_shared_memory_pages.has_value() &&
           *lim.max_total_shared_memory_pages == 1024ULL,
           "shmall preserved");
    expect(lim.max_shared_memory_segments_system.has_value() &&
           *lim.max_shared_memory_segments_system == 32ULL,
           "shmmni preserved");
    expect(lim.max_message_bytes.has_value() &&
           *lim.max_message_bytes == 2048ULL,
           "msgmax preserved");
    expect(lim.default_message_queue_bytes.has_value() &&
           *lim.default_message_queue_bytes == 4096ULL,
           "msgmnb preserved");
    expect(lim.max_message_queues_system.has_value() &&
           *lim.max_message_queues_system == 16ULL,
           "msgmni preserved");
    expect(lim.max_semaphores_system.has_value() &&
           *lim.max_semaphores_system == 256ULL,
           "semmns preserved");
    expect(lim.max_semaphores_per_set.has_value() &&
           *lim.max_semaphores_per_set == 32U,
           "semmsl preserved");
}

void test_macos_ipc_backend() {
    const auto shm = syscape::ipc::shared_memory_segments();
    expect(!shm && shm.error() == syscape::errc::not_supported,
           "SysV/POSIX shm must report not_supported on unprivileged macOS");

    const auto mq = syscape::ipc::message_queues();
    expect(!mq && mq.error() == syscape::errc::not_supported,
           "SysV/POSIX msg queues must report not_supported on unprivileged macOS");

    const auto sem = syscape::ipc::semaphore_sets();
    expect(!sem && sem.error() == syscape::errc::not_supported,
           "SysV/POSIX semaphores must report not_supported on unprivileged macOS");

    const auto socks = syscape::ipc::local_sockets();
    expect(!socks && socks.error() == syscape::errc::not_supported,
           "local_sockets must report not_supported on macOS");

    const auto lim = syscape::ipc::limits();
#if defined(__APPLE__) && defined(__MACH__)
    expect(lim.has_value() ||
           lim.error() == syscape::errc::permission_denied ||
           lim.error() == syscape::errc::not_supported,
           "limits returns valid limits or documented platform error on macOS");
#else
    expect(!lim && lim.error() == syscape::errc::not_supported,
           "limits returns not_supported on non-macOS host");
#endif
}

} // namespace

int main() {
    test_macos_synthetic_ipc_limits();
    test_macos_ipc_backend();
    return failures == 0 ? 0 : 1;
}
