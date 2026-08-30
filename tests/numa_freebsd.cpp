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
    const auto count = syscape::numa::node_count();
    expect(count && *count >= 1, "node_count must be at least 1");

    const auto nodes = syscape::numa::nodes();
    expect(nodes && !nodes->empty(), "nodes must not be empty");
}

} // namespace

int main() {
    test_numa_queries();
    return failures == 0 ? 0 : 1;
}
