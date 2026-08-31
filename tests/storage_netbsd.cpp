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
    const auto drives = syscape::storage::drives();
    expect(drives.has_value() || drives.error() == syscape::errc::not_supported,
           "physical drives query must succeed or report not_supported");
}

} // namespace

int main() {
    test_storage_queries();
    return failures == 0 ? 0 : 1;
}
