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
    expect(avail.has_value() || avail.error() == syscape::errc::not_supported,
           "numa availability query must succeed or report not_supported");

    const auto count = syscape::numa::node_count();
    expect(count.has_value() || count.error() == syscape::errc::not_supported,
           "numa node count query must succeed or report not_supported");
}

} // namespace

int main() {
    test_numa_queries();
    return failures == 0 ? 0 : 1;
}
