#include <iostream>

#include <syscape/resource.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_resource_queries() {
    const auto loads = syscape::resource::load_average();
    expect(loads.has_value(), "load average query must succeed");

    const auto max_files = syscape::resource::file_descriptor_limit();
    expect(max_files && *max_files > 0,
           "file descriptor limit must be positive");
}

} // namespace

int main() {
    test_resource_queries();
    return failures == 0 ? 0 : 1;
}
