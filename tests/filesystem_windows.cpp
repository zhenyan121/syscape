#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include <syscape/detail/filesystem/common.hpp>
#include <syscape/detail/filesystem/windows.hpp>
#include <syscape/filesystem.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct ready_drive_api {
    bool fail_logical_drives = false;
    bool fail_dos_device = false;
    ::DWORD mask = (1U << 0) | (1U << 2) | (1U << 25);

    syscape::result<::DWORD> logical_drives() const {
        if (fail_logical_drives) {
            return syscape::fail(std::error_code(5, std::system_category()));
        }
        return mask;
    }

    syscape::result<std::wstring> dos_device(
        const std::wstring& letter) const {
        if (fail_dos_device) {
            return syscape::fail(std::error_code(1, std::system_category()));
        }
        return letter + L"\\Device\\HarddiskVolume";
    }

    syscape::result<std::wstring> file_system_name(
        const std::wstring& root) const {
        if (root == L"C:\\") { return std::wstring(); }
        if (root == L"Z:\\") { return std::wstring(L"FAT32"); }
        return std::wstring(L"NTFS");
    }
};

void test_drive_enumeration() {
    const auto mounted =
        syscape::detail::filesystem_backend::enumerate_drive_mounts(
            ready_drive_api{});
    expect(mounted && mounted->size() == 2U,
           "Letters without a queryable file system are omitted");

    expect(mounted && (*mounted)[0U].mount_point == "A:\\",
           "The first ready drive keeps its letter as mount point");
    expect(mounted && (*mounted)[0U].source ==
                          "A:\\Device\\HarddiskVolume",
           "The DOS device mapping becomes the source field");
    expect(mounted && (*mounted)[1U].mount_point == "Z:\\" &&
               (*mounted)[1U].file_system_type == "FAT32",
           "Each ready drive carries its own reported file-system name");

    ready_drive_api denied;
    denied.fail_logical_drives = true;
    const auto drive_failure =
        syscape::detail::filesystem_backend::enumerate_drive_mounts(denied);
    expect(!drive_failure &&
               drive_failure.error() ==
                   std::error_code(5, std::system_category()),
           "A failed drive-bitmask query preserves its native error");

    ready_drive_api no_mapping;
    no_mapping.fail_dos_device = true;
    const auto mapping_failure =
        syscape::detail::filesystem_backend::enumerate_drive_mounts(
            no_mapping);
    expect(!mapping_failure &&
               mapping_failure.error() ==
                   std::error_code(1, std::system_category()),
           "An API-level device-mapping failure is preserved, not hidden");

    ready_drive_api silent;
    silent.mask = 1U << 2;
    const auto empty_type =
        syscape::detail::filesystem_backend::enumerate_drive_mounts(silent);
    expect(empty_type && empty_type->empty(),
           "A letter whose volume reports an empty file-system name is "
           "omitted like unready media");
}

void test_runtime_space() {
    const auto relative = syscape::filesystem::space(".");
    expect(relative.has_value(),
           "A relative path resolves against the current working directory");

    const auto missing = syscape::filesystem::space(
        "syscape-filesystem-definitely-missing-4711\\child");
    expect(!missing &&
               missing.error() == std::errc::no_such_file_or_directory,
           "A missing Windows path preserves its native not-found error");
}

} // namespace

int main() {
    test_drive_enumeration();
    test_runtime_space();
    return failures == 0 ? 0 : 1;
}
