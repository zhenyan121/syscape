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
           "is_numa_available on OpenBSD must return not_supported");

    const auto count = syscape::numa::node_count();
    expect(!count && count.error() == syscape::errc::not_supported,
           "node_count on OpenBSD must return not_supported");

    const auto nodes = syscape::numa::nodes();
    expect(!nodes && nodes.error() == syscape::errc::not_supported,
           "nodes on OpenBSD must return not_supported");

    const auto current = syscape::numa::current_thread_node();
    expect(!current && current.error() == syscape::errc::not_supported,
           "current_thread_node must return not_supported");
}

} // namespace

int main() {
    test_numa_queries();
    return failures == 0 ? 0 : 1;
}
