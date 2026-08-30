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
    const auto limits = syscape::resource::file_descriptor_limit();
    expect(limits.has_value(), "file descriptor limit query must succeed");
}

} // namespace

int main() {
    test_resource_queries();
    return failures == 0 ? 0 : 1;
}
