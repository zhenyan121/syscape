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
    expect(
        !shm && shm.error() == syscape::errc::not_supported,
        "shared memory segments query must report not_supported on SerenityOS");

    const auto limits = syscape::ipc::limits();
    expect(!limits && limits.error() == syscape::errc::not_supported,
           "limits query must report not_supported on SerenityOS");
}

} // namespace

int main() {
    test_ipc_queries();
    return failures == 0 ? 0 : 1;
}
