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
               drvs.error() == syscape::errc::not_found,
           "drives query must return list, not_supported, or not_found");
}

} // namespace

int main() {
    test_storage_queries();
    return failures == 0 ? 0 : 1;
}
