#include <cstdint>
#include <iostream>
#include <system_error>

#include <syscape/detail/numa/common.hpp>
#include <syscape/numa.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_windows_validation() {
    syscape::numa::numa_node node;
    node.id = 0U;
    node.is_online = true;
    node.free_memory_bytes = 1048576U;
    node.distances = {}; // Windows does not expose inter-node distance matrix
    node.logical_processors = {0U, 1U};

    const auto valid = syscape::detail::numa_common::validate_numa_node(node);
    expect(valid.has_value(), "Valid Windows numa node passes validation");
}

} // namespace

int main() {
    test_windows_validation();
    return failures == 0 ? 0 : 1;
}
