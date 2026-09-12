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
           "drives must report not_supported on INTEGRITY");

    const auto partitions = syscape::storage::partitions();
    expect(!partitions && partitions.error() == syscape::errc::not_supported,
           "partitions must report not_supported on INTEGRITY");

    const auto disk_parts = syscape::storage::disk_partitions("disk0");
    expect(!disk_parts && disk_parts.error() == syscape::errc::not_supported,
           "disk_partitions must report not_supported on INTEGRITY");

    const auto health = syscape::storage::health("disk0");
    expect(!health && health.error() == syscape::errc::not_supported,
           "health must report not_supported on INTEGRITY");

    const auto all_health = syscape::storage::all_drive_health();
    expect(!all_health && all_health.error() == syscape::errc::not_supported,
           "all_drive_health must report not_supported on INTEGRITY");
}

} // namespace

int main() {
    test_storage_queries();
    return failures == 0 ? 0 : 1;
}
