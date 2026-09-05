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
    expect(!drives && drives.error() == syscape::errc::not_supported,
           "drives query must report not_supported on GNU/Hurd");

    const auto partitions = syscape::storage::partitions();
    expect(!partitions && partitions.error() == syscape::errc::not_supported,
           "partitions query must report not_supported on GNU/Hurd");

    const auto health = syscape::storage::all_drive_health();
    expect(!health && health.error() == syscape::errc::not_supported,
           "all_drive_health query must report not_supported on GNU/Hurd");
}

} // namespace

int main() {
    test_storage_queries();
    return failures == 0 ? 0 : 1;
}
