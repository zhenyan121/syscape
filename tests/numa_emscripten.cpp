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
    const auto nodes = syscape::numa::nodes();
    expect(!nodes && nodes.error() == syscape::errc::not_supported,
           "numa nodes must report not_supported on Emscripten");
}

} // namespace

int main() {
    test_numa_queries();
    return failures == 0 ? 0 : 1;
}
