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

void test_volume_facts_queries() {
    struct ready_volume_api {
        bool fail_information = false;
        std::uint32_t serial = 0x1234abcdU;
        std::uint64_t component = 255U;

        syscape::result<syscape::detail::filesystem_backend::volume_facts>
        volume_information(const std::wstring&) const {
            if (fail_information) {
                return syscape::fail(
                    std::error_code(5, std::system_category()));
            }
            syscape::detail::filesystem_backend::volume_facts facts;
            facts.serial_number = serial;
            facts.max_component_length = component;
            return facts;
        }
    };

    // The resolution half of these helpers contacts the real filesystem
    // for an existing path, while all volume information comes from the
    // injected facts.
    const auto component =
        syscape::detail::filesystem_backend::component_length_via(
            ready_volume_api{}, ".");
    expect(component && !component->indeterminate &&
               component->length == 255U,
           "The recorded MaximumComponentLength value is preserved "
           "verbatim in UTF-16 code units");

    const auto identifier =
        syscape::detail::filesystem_backend::volume_id_via(
            ready_volume_api{}, ".");
    expect(identifier && *identifier == "1234abcd",
           "The serial word renders as eight lowercase hexadecimal "
           "digits");

    ready_volume_api anonymous;
    anonymous.serial = 0U;
    const auto zero_identifier =
        syscape::detail::filesystem_backend::volume_id_via(anonymous, ".");
    expect(zero_identifier && *zero_identifier == "00000000",
           "An all-zero serial recording renders verbatim as valid "
           "data");

    ready_volume_api denied;
    denied.fail_information = true;
    const auto failed =
        syscape::detail::filesystem_backend::component_length_via(denied,
                                                                  ".");
    expect(!failed &&
               failed.error() == std::error_code(5, std::system_category()),
           "A failed volume-information query preserves its native "
           "error");

    const auto failed_identifier =
        syscape::detail::filesystem_backend::volume_id_via(denied, ".");
    expect(!failed_identifier &&
               failed_identifier.error() ==
                   std::error_code(5, std::system_category()),
           "The identifier query preserves the native error too");

    ready_volume_api degenerate;
    degenerate.component = 0U;
    const auto zero_component =
        syscape::detail::filesystem_backend::component_length_via(
            degenerate, ".");
    const auto validated =
        syscape::detail::filesystem_common::validate_path_length(
            zero_component);
    expect(!validated &&
               validated.error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "A determinate component bound of zero is malformed platform "
           "data at the public boundary");

    const auto unsupported =
        syscape::detail::filesystem_backend::max_path_length(".");
    expect(!unsupported &&
               unsupported.error() ==
                   syscape::make_error_code(syscape::errc::not_supported),
           "Windows exposes no per-volume complete-path bound and "
           "reports not_supported");
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
    test_volume_facts_queries();
    test_runtime_space();
    return failures == 0 ? 0 : 1;
}
