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
    const auto s = syscape::ipc::shared_memory_segments();
    expect(!s && s.error() == syscape::errc::not_supported,
           "shared_memory_segments query must report not_supported on Haiku");
}

} // namespace

int main() {
    test_ipc_queries();
    return failures == 0 ? 0 : 1;
}
