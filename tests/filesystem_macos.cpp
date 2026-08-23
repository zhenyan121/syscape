#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include <sys/statvfs.h>
#include <unistd.h>

#include <syscape/detail/filesystem/common.hpp>
#include <syscape/detail/filesystem/macos.hpp>
#include <syscape/detail/filesystem/posix.hpp>
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

void test_pathconf_conversion() {
    namespace backend = syscape::detail::filesystem_backend;

    const auto plain = backend::convert_pathconf_outcome(255L, 0);
    expect(plain && !plain->indeterminate && plain->length == 255U,
           "A positive pathconf record converts to a determinate bound");

    const auto indeterminate = backend::convert_pathconf_outcome(-1L, 0);
    expect(indeterminate && indeterminate->indeterminate &&
               indeterminate->length == 0U,
           "A -1 return with unchanged errno records an indeterminate "
           "limit as valid data");

    const auto failed = backend::convert_pathconf_outcome(-1L, ENOENT);
    expect(!failed &&
               failed.error() ==
                   std::error_code(ENOENT, std::generic_category()),
           "A -1 return with a stored errno preserves the native error");

    const auto zero_bound = backend::convert_pathconf_outcome(0L, 0);
    expect(!zero_bound &&
               zero_bound.error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "A determinate bound of zero is malformed platform data");
}

void test_hex_rendering() {
    using syscape::detail::filesystem_common::render_hex32;
    using syscape::detail::filesystem_common::render_hex_word_pair;

    expect(render_hex32(0U) == "00000000",
           "Zero renders at full fixed width");
    expect(render_hex32(0xffffffffU) == "ffffffff",
           "The maximum word renders as eight lowercase digits");
    expect(render_hex_word_pair(1U, 0xdeadbeefU) == "00000001deadbeef",
           "Word pairs render first word then second at fixed width");
}

void test_runtime_limits_and_identifier() {
    constexpr const char* probe = "/";

    errno = 0;
    const long name_reference = ::pathconf(probe, _PC_NAME_MAX);
    const int name_errno = name_reference == -1 ? errno : 0;
    const auto component =
        syscape::filesystem::max_component_length(probe);
    if (name_reference == -1 && name_errno != 0) {
        expect(!component &&
                   component.error() ==
                       std::error_code(name_errno, std::generic_category()),
               "A failing component-length reference matches the query "
               "error");
    } else if (name_reference == -1) {
        expect(component && component->indeterminate,
               "An indeterminate reference limit must reach the explicit "
               "flag");
    } else {
        expect(component && !component->indeterminate &&
                   component->length ==
                       static_cast<std::uint64_t>(name_reference),
               "The component bound must match an independent pathconf "
               "record");
    }

    errno = 0;
    const long path_reference = ::pathconf(probe, _PC_PATH_MAX);
    const int path_errno = path_reference == -1 ? errno : 0;
    const auto whole_path = syscape::filesystem::max_path_length(probe);
    if (path_reference == -1 && path_errno != 0) {
        expect(!whole_path &&
                   whole_path.error() ==
                       std::error_code(path_errno, std::generic_category()),
               "A failing complete-path reference matches the query error");
    } else if (path_reference == -1) {
        expect(whole_path && whole_path->indeterminate,
               "An indeterminate complete-path reference reaches the "
               "explicit flag");
    } else {
        expect(whole_path && !whole_path->indeterminate &&
                   whole_path->length ==
                       static_cast<std::uint64_t>(path_reference),
               "The complete-path bound must match an independent "
               "pathconf record");
    }

    struct ::statfs reference {};
    expect(::statfs(probe, &reference) == 0,
           "The statfs reference lookup must not fail natively");
    std::uint32_t first = 0U;
    std::uint32_t second = 0U;
    ::memcpy(&first, &reference.f_fsid.val[0], sizeof(first));
    ::memcpy(&second, &reference.f_fsid.val[1], sizeof(second));

    const auto identifier = syscape::filesystem::volume_id(probe);
    expect(identifier && identifier->size() == 16U,
           "The identifier renders at the documented fixed width");
    expect(identifier &&
               *identifier == syscape::detail::filesystem_common::
                                  render_hex_word_pair(first, second),
           "The rendering must match the recorded statfs word pair in "
           "documented order");

    // Whether Darwin's limit interface validates the whole path before
    // answering is compared against an independent reference instead of
    // being assumed.
    ::errno = 0;
    const long missing_reference =
        ::pathconf("/definitely-not-present-4711", _PC_NAME_MAX);
    const int missing_errno = missing_reference == -1 ? errno : 0;
    const auto missing =
        syscape::filesystem::max_component_length(
            "/definitely-not-present-4711");
    if (missing_reference == -1 && missing_errno != 0) {
        expect(!missing &&
                   missing.error() ==
                       std::error_code(missing_errno,
                                       std::generic_category()),
               "A failing reference record matches the query outcome");
    } else if (missing_reference == -1) {
        expect(missing && missing->indeterminate,
               "An indeterminate missing-path reference reaches the "
               "explicit flag");
    } else {
        expect(missing && !missing->indeterminate &&
                   missing->length ==
                       static_cast<std::uint64_t>(missing_reference),
               "A reference record answered without path validation "
               "matches the query value");
    }
}

} // namespace

int main() {
    test_statfs_conversion();
    test_pathconf_conversion();
    test_hex_rendering();
    test_runtime_mounts();
    test_runtime_space();
    test_runtime_limits_and_identifier();
    return failures == 0 ? 0 : 1;
}
