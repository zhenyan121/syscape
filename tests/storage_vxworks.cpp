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
    const auto drvs = syscape::storage::drives();
    expect(drvs.error() == syscape::errc::not_supported,
           "drives query must report not_supported on VxWorks");

    const auto parts = syscape::storage::partitions();
    expect(parts.error() == syscape::errc::not_supported,
           "partitions query must report not_supported on VxWorks");
}

} // namespace

int main() {
    test_storage_queries();
    return failures == 0 ? 0 : 1;
}
