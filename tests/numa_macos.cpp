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

void test_macos_uma_model() {
    syscape::numa::numa_node node;
    node.id = 0U;
    node.is_online = true;
    node.total_memory_bytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;
    node.free_memory_bytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
    node.distances = {}; // macOS UMA does not expose inter-node distance matrix
    node.logical_processors = {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U};

    const auto valid = syscape::detail::numa_common::validate_numa_node(node);
    expect(valid.has_value(), "Valid macOS single-node model passes validation");
}

} // namespace

int main() {
    test_macos_uma_model();
    return failures == 0 ? 0 : 1;
}
