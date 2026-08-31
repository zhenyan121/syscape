#include <iostream>

#include <syscape/filesystem.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_filesystem_queries() {
    const auto mounts = syscape::filesystem::mounts();
    expect(mounts && !mounts->empty(), "mounts must return at least one entry");

    const auto root_space = syscape::filesystem::space("/");
    expect(root_space.has_value(), "root space query must succeed");
}

} // namespace

int main() {
    test_filesystem_queries();
    return failures == 0 ? 0 : 1;
}
