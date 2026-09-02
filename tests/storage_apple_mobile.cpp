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
    expect(drvs.has_value() || drvs.error() == syscape::errc::not_supported ||
               drvs.error() == syscape::errc::permission_denied,
           "drives query must succeed or report expected error");

    const auto parts = syscape::storage::partitions();
    expect(parts.has_value() || parts.error() == syscape::errc::not_supported ||
               parts.error() == syscape::errc::permission_denied,
           "partitions query must succeed or report expected error");
}

} // namespace

int main() {
    test_storage_queries();
    return failures == 0 ? 0 : 1;
}
