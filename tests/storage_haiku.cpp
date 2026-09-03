#include <iostream>

#include <syscape/storage.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_storage_queries() {
    const auto d = syscape::storage::drives();
    expect(!d && d.error() == syscape::errc::not_supported,
           "drives query must report not_supported on Haiku");
}

} // namespace

int main() {
    test_storage_queries();
    return failures == 0 ? 0 : 1;
}
