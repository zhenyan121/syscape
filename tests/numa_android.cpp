#include <iostream>

#include <syscape/numa.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_numa_queries() {
    const auto available = syscape::numa::is_numa_available();
    expect(!available && available.error() == syscape::errc::not_supported,
           "NUMA availability must report not_supported on Android");

    const auto count = syscape::numa::node_count();
    expect(!count && count.error() == syscape::errc::not_supported,
           "NUMA node count must report not_supported on Android");
}

} // namespace

int main() {
    test_numa_queries();
    return failures == 0 ? 0 : 1;
}
