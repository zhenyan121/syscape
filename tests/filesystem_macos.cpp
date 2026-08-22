#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include <sys/statvfs.h>

#include <syscape/detail/filesystem/common.hpp>
#include <syscape/detail/filesystem/macos.hpp>
#include <syscape/filesystem.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_statfs_conversion() {
    struct ::statfs entry {};
    ::strncpy(entry.f_mntfromname, "/dev/disk1s1",
              sizeof(entry.f_mntfromname) - 1U);
    ::strncpy(entry.f_mntonname, "/",
              sizeof(entry.f_mntonname) - 1U);
    ::strncpy(entry.f_fstypename, "apfs",
              sizeof(entry.f_fstypename) - 1U);

    const auto converted =
        syscape::detail::filesystem_backend::convert_statfs_entry(entry);
    expect(converted && converted->source == "/dev/disk1s1" &&
               converted->mount_point == "/" &&
               converted->file_system_type == "apfs",
           "A filled statfs record converts field for field");
}

void test_runtime_mounts() {
    const auto mounted = syscape::filesystem::mounts();
    expect(mounted && !mounted->empty(),
           "A running macOS always exposes at least one mount");

    bool root_listed = false;
    for (const syscape::filesystem::mount_entry& entry : *mounted) {
        expect(!entry.mount_point.empty() && entry.mount_point.front() == '/',
               "Every macOS mount point is absolute");
        expect(!entry.file_system_type.empty(),
               "Every macOS record carries a file-system type");
        if (entry.mount_point == "/") { root_listed = true; }
    }
    expect(root_listed, "The root filesystem is always mounted on macOS");
}

void test_runtime_space() {
    struct ::statvfs reference {};
    expect(::statvfs("/", &reference) == 0,
           "The statvfs reference lookup must not fail natively");

    const auto queried = syscape::filesystem::space("/");
    if (!queried) {
        expect(false, "A live space query on / must succeed on macOS");
        return;
    }
    expect(queried->capacity_bytes ==
               static_cast<std::uint64_t>(reference.f_blocks) *
                   static_cast<std::uint64_t>(reference.f_frsize),
           "Capacity must match statvfs scaled by f_frsize");
    expect(queried->block_size_bytes ==
               static_cast<std::uint64_t>(reference.f_frsize),
           "The block size must be the fundamental f_frsize value");

    const auto missing =
        syscape::filesystem::space("/definitely-not-present-4711");
    expect(!missing && missing.error() ==
                           std::error_code(ENOENT, std::generic_category()),
           "A missing path preserves its native ENOENT error");
}

} // namespace

int main() {
    test_statfs_conversion();
    test_runtime_mounts();
    test_runtime_space();
    return failures == 0 ? 0 : 1;
}
