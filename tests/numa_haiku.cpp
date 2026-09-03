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
    const auto avail = syscape::numa::is_numa_available();
    expect(!avail && avail.error() == syscape::errc::not_supported,
           "is_numa_available query must report not_supported on Haiku");
}

} // namespace

int main() {
    test_numa_queries();
    return failures == 0 ? 0 : 1;
}
