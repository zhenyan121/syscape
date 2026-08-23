#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include <stdio.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <mntent.h>

#include <syscape/detail/filesystem/common.hpp>
#include <syscape/detail/filesystem/linux.hpp>
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

void expect_malformed(const std::string& input, const char* message) {
    const auto parsed =
        syscape::detail::filesystem_backend::parse_mounts(input);
    expect(!parsed && parsed.error() ==
                          syscape::make_error_code(
                              syscape::errc::malformed_data),
           message);
}

void test_parse_basic_records() {
    const auto empty =
        syscape::detail::filesystem_backend::parse_mounts("");
    expect(empty && empty->empty(),
           "An empty mount table is valid and produces no records");

    const auto single = syscape::detail::filesystem_backend::parse_mounts(
        "sysfs /sys sysfs rw,nosuid,nodev,noexec,relatime 0 0\n");
    expect(single && single->size() == 1U &&
               (*single)[0U].source == "sysfs" &&
               (*single)[0U].mount_point == "/sys" &&
               (*single)[0U].file_system_type == "sysfs",
           "One documented record must parse into its three used fields");

    const auto multiple =
        syscape::detail::filesystem_backend::parse_mounts(
            "proc /proc proc rw,nosuid,nodev,noexec,relatime 0 0\n"
            "\n"
            "tmpfs /dev/shm tmpfs rw 0 0");
    expect(multiple && multiple->size() == 2U &&
               (*multiple)[0U].mount_point == "/proc" &&
               (*multiple)[1U].mount_point == "/dev/shm",
           "Blank lines are skipped between valid records");

    const auto extended =
        syscape::detail::filesystem_backend::parse_mounts(
            "proc /proc proc rw 0 0 extra future fields\n");
    expect(extended && extended->size() == 1U,
           "Extra trailing fields belong to future extensions and are "
           "ignored");
}

void test_parse_escapes() {
    const auto escaped = syscape::detail::filesystem_backend::parse_mounts(
        "/dev/sda1 /mnt/with\\040space ext4 rw 0 0\n"
        "/dev/tab\\011by /t ext4 rw 0 0\n"
        "\\134backslash /b ext4 rw 0 0\n"
        "/dev/new\\012line /n ext4 rw 0 0\n");
    expect(escaped && escaped->size() == 4U &&
               (*escaped)[0U].mount_point == "/mnt/with space" &&
               (*escaped)[1U].source == "/dev/tab\tby" &&
               (*escaped)[2U].source == "\\backslash" &&
               (*escaped)[3U].source == "/dev/new\nline",
           "The four kernel-documented octal escapes must decode");

    expect_malformed("/dev/sda1 /mnt/truncated\\04 ext4 rw 0 0\n",
                     "A truncated escape is malformed platform data");
    expect_malformed("/dev/sda1 /mnt/bad\\128 ext4 rw 0 0\n",
                     "A non-octal escape digit is malformed platform data");
    expect_malformed("/dev/sda1 /mnt/undocumented\\101 ext4 rw 0 0\n",
                     "An undocumented escape code is malformed platform "
                     "data");
}

void test_parse_malformed_records() {
    expect_malformed("only three fields here\n",
                     "A record without six fields is malformed platform "
                     "data");
    expect_malformed("one\n", "A one-field record is malformed platform data");

    const auto bad_field =
        syscape::detail::filesystem_backend::parse_mounts(
            "dev\\bad /mount ext4 rw 0 0\n");
    expect(!bad_field && bad_field.error() ==
                             syscape::make_error_code(
                                 syscape::errc::malformed_data),
           "A malformed required field fails the whole snapshot instead of "
           "dropping entries silently");
}

void test_space_snapshot_computation() {
    struct ::statvfs status {};
    status.f_bsize = 4096U;
    status.f_frsize = 4096U;
    status.f_blocks = 1000U;
    status.f_bfree = 400U;
    status.f_bavail = 300U;
    status.f_flag = ST_RDONLY;

    const auto snapshot =
        syscape::detail::filesystem_backend::compute_space_snapshot(status);
    expect(snapshot && snapshot->capacity_bytes == 4096ULL * 1000ULL &&
               snapshot->free_bytes == 4096ULL * 400ULL &&
               snapshot->available_bytes == 4096ULL * 300ULL &&
               snapshot->block_size_bytes == 4096ULL &&
               snapshot->read_only,
           "statvfs counters must scale by the fundamental block size and "
           "report the read-only flag");

    struct ::statvfs fallback_to_bsize {};
    fallback_to_bsize.f_bsize = 512U;
    fallback_to_bsize.f_frsize = 0U;
    fallback_to_bsize.f_blocks = 2U;
    fallback_to_bsize.f_bfree = 1U;
    fallback_to_bsize.f_bavail = 1U;
    const auto bsize_used =
        syscape::detail::filesystem_backend::compute_space_snapshot(
            fallback_to_bsize);
    expect(bsize_used && bsize_used->block_size_bytes == 512U,
           "POSIX requires f_bsize when f_frsize records zero");

    struct ::statvfs degenerate {};
    degenerate.f_bsize = 0U;
    degenerate.f_frsize = 0U;
    degenerate.f_blocks = 10U;
    degenerate.f_bfree = 5U;
    degenerate.f_bavail = 5U;
    const auto rejected =
        syscape::detail::filesystem_backend::compute_space_snapshot(
            degenerate);
    expect(!rejected && rejected.error() ==
                           syscape::make_error_code(
                               syscape::errc::malformed_data),
           "A volume without a usable block size cannot express bytes and "
           "is malformed platform data");

    struct ::statvfs oversized {};
    oversized.f_bsize = 8U;
    oversized.f_frsize = 8U;
    oversized.f_blocks = static_cast<fsblkcnt_t>(-1);
    oversized.f_bfree = 1U;
    oversized.f_bavail = 1U;
    const auto too_large =
        syscape::detail::filesystem_backend::compute_space_snapshot(
            oversized);
    expect(!too_large && too_large.error() ==
                             syscape::make_error_code(
                                 syscape::errc::value_too_large),
           "A byte total beyond 64 bits reports value_too_large instead of "
           "wrapping");
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

    const auto below_contract = backend::convert_pathconf_outcome(-2L, 0);
    expect(!below_contract &&
               below_contract.error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "A value below the documented -1 contract is malformed "
           "platform data");

    const auto zero_bound = backend::convert_pathconf_outcome(0L, 0);
    expect(!zero_bound &&
               zero_bound.error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "A determinate bound of zero cannot name any component and "
           "is malformed platform data");
}

void test_hex_rendering() {
    using syscape::detail::filesystem_common::render_hex32;
    using syscape::detail::filesystem_common::render_hex_word_pair;

    expect(render_hex32(0U) == "00000000",
           "Zero renders at full fixed width");
    expect(render_hex32(0xffffffffU) == "ffffffff",
           "The maximum word renders as eight lowercase digits");
    expect(render_hex32(0x1234abcdU) == "1234abcd",
           "Digits render in conventional high-to-low order");
    expect(render_hex_word_pair(1U, 0xdeadbeefU) == "00000001deadbeef",
           "Word pairs render first word then second at fixed width");
}

void test_common_validation() {
    using syscape::detail::filesystem_common::mount_record;
    using syscape::detail::filesystem_common::space_snapshot;

    std::vector<mount_record> records(1U);
    records[0U].source = "dev";
    records[0U].mount_point = "/mnt";
    records[0U].file_system_type = "ext4";
    const auto accepted =
        syscape::detail::filesystem_common::validate_mount_records(
            syscape::result<std::vector<mount_record>>(records));
    expect(static_cast<bool>(accepted) && accepted->size() == 1U,
           "A complete record passes boundary validation");

    std::vector<mount_record> anonymous(1U);
    anonymous[0U].mount_point = "/proc";
    anonymous[0U].file_system_type = "proc";
    const auto sourceless =
        syscape::detail::filesystem_common::validate_mount_records(
            syscape::result<std::vector<mount_record>>(anonymous));
    expect(static_cast<bool>(sourceless),
           "An empty source is valid where the platform records no device");

    std::vector<mount_record> nameless(1U);
    nameless[0U].source = "dev";
    nameless[0U].file_system_type = "ext4";
    const auto no_point =
        syscape::detail::filesystem_common::validate_mount_records(
            syscape::result<std::vector<mount_record>>(nameless));
    expect(!no_point && no_point.error() ==
                            syscape::make_error_code(
                                syscape::errc::malformed_data),
           "An empty mount point is malformed platform data");

    std::vector<mount_record> undecodable(1U);
    undecodable[0U].mount_point = std::string("/tmp/\xffx", 7U);
    undecodable[0U].file_system_type = "ext4";
    const auto invalid_encoding =
        syscape::detail::filesystem_common::validate_mount_records(
            syscape::result<std::vector<mount_record>>(undecodable));
    expect(!invalid_encoding &&
               invalid_encoding.error() ==
                   syscape::make_error_code(syscape::errc::invalid_encoding),
           "Non-UTF-8 record text must fail at the public boundary");

    space_snapshot inconsistent;
    inconsistent.block_size_bytes = 4096U;
    inconsistent.capacity_bytes = 100U;
    inconsistent.free_bytes = 101U;
    inconsistent.available_bytes = 99U;
    const auto over_free =
        syscape::detail::filesystem_common::validate_space_snapshot(
            syscape::result<space_snapshot>(inconsistent));
    expect(!over_free && over_free.error() ==
                             syscape::make_error_code(
                                 syscape::errc::malformed_data),
           "Free capacity beyond total capacity contradicts the platform's "
           "own accounting");

    space_snapshot over_available;
    over_available.block_size_bytes = 4096U;
    over_available.capacity_bytes = 100U;
    over_available.free_bytes = 50U;
    over_available.available_bytes = 101U;
    const auto over_avail =
        syscape::detail::filesystem_common::validate_space_snapshot(
            syscape::result<space_snapshot>(over_available));
    expect(!over_avail &&
               over_avail.error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "Available capacity beyond total capacity is malformed platform "
           "data");

    space_snapshot consistent;
    consistent.block_size_bytes = 4096U;
    consistent.capacity_bytes = 100U;
    consistent.free_bytes = 50U;
    consistent.available_bytes = 50U;
    consistent.read_only = true;
    const auto good =
        syscape::detail::filesystem_common::validate_space_snapshot(
            syscape::result<space_snapshot>(consistent));
    expect(static_cast<bool>(good) && good->read_only,
           "A consistent snapshot including read-only state passes");

    using syscape::detail::filesystem_common::path_length_snapshot;

    path_length_snapshot determinate;
    determinate.length = 255U;
    const auto accepted_bound =
        syscape::detail::filesystem_common::validate_path_length(
            syscape::result<path_length_snapshot>(determinate));
    expect(static_cast<bool>(accepted_bound) &&
               accepted_bound->length == 255U,
           "A determinate nonzero bound passes boundary validation");

    path_length_snapshot no_fixed_bound;
    no_fixed_bound.indeterminate = true;
    no_fixed_bound.length = 7U;
    const auto normalized =
        syscape::detail::filesystem_common::validate_path_length(
            syscape::result<path_length_snapshot>(no_fixed_bound));
    expect(normalized && normalized->indeterminate &&
               normalized->length == 0U,
           "An indeterminate bound is valid data and normalizes its "
           "length to zero");

    path_length_snapshot zero_bound;
    zero_bound.length = 0U;
    const auto rejected_bound =
        syscape::detail::filesystem_common::validate_path_length(
            syscape::result<path_length_snapshot>(zero_bound));
    expect(!rejected_bound &&
               rejected_bound.error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "A determinate bound of zero is malformed platform data");

    const auto empty_identifier =
        syscape::detail::filesystem_common::validate_volume_id(
            syscape::result<std::string>(std::string()));
    expect(!empty_identifier &&
               empty_identifier.error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "An empty identifier rendering means the backend recorded "
           "nothing and is malformed platform data");

    const auto zero_identifier =
        syscape::detail::filesystem_common::validate_volume_id(
            syscape::result<std::string>(std::string("0000000000000000")));
    expect(static_cast<bool>(zero_identifier),
           "An all-zero identifier rendering is valid platform data");
}

std::vector<syscape::detail::filesystem_common::mount_record>
reference_mount_table() {
    // getmntent decodes the same octal escapes as the backend, so it is an
    // independent reference decoder for the live table.
    std::vector<syscape::detail::filesystem_common::mount_record> reference;
    ::FILE* table = ::setmntent("/proc/self/mounts", "r");
    if (table == nullptr) { return reference; }
    for (;;) {
        const ::mntent* entry = ::getmntent(table);
        if (entry == nullptr) { break; }
        syscape::detail::filesystem_common::mount_record record;
        record.source = entry->mnt_fsname != nullptr ? entry->mnt_fsname : "";
        record.mount_point = entry->mnt_dir != nullptr ? entry->mnt_dir : "";
        record.file_system_type =
            entry->mnt_type != nullptr ? entry->mnt_type : "";
        reference.push_back(std::move(record));
    }
    ::endmntent(table);
    return reference;
}

void test_runtime_mounts() {
    const auto mounted = syscape::filesystem::mounts();
    expect(mounted && !mounted->empty(),
           "A running hosted Linux always exposes at least one mount");

    bool root_listed = false;
    for (const syscape::filesystem::mount_entry& entry : *mounted) {
        expect(!entry.mount_point.empty() && entry.mount_point.front() == '/',
               "Every Linux mount point is absolute");
        expect(!entry.file_system_type.empty(),
               "Every Linux record carries a file-system type");
        if (entry.mount_point == "/") { root_listed = true; }
    }
    expect(root_listed, "The root filesystem is always mounted on Linux");

    const std::vector<syscape::detail::filesystem_common::mount_record>
        reference = reference_mount_table();
    expect(reference.size() == mounted->size(),
           "Enumeration must match an independent decoder of the same "
           "table");
    const std::size_t shared =
        reference.size() < mounted->size() ? reference.size()
                                           : mounted->size();
    for (std::size_t index = 0; index < shared; ++index) {
        expect((*mounted)[index].source == reference[index].source &&
                   (*mounted)[index].mount_point ==
                       reference[index].mount_point &&
                   (*mounted)[index].file_system_type ==
                       reference[index].file_system_type,
               "Decoded fields must match the independent reference "
               "decoder entry for entry");
    }
}

void test_runtime_space() {
    constexpr const char* probe = "/tmp";

    struct ::statvfs reference {};
    expect(::statvfs(probe, &reference) == 0,
           "The statvfs reference lookup must not fail natively");

    const auto queried = syscape::filesystem::space(probe);
    if (!queried) {
        expect(false, "A live space query on /tmp must succeed on Linux");
        return;
    }
    expect(queried->capacity_bytes ==
               static_cast<std::uint64_t>(reference.f_blocks) *
                   static_cast<std::uint64_t>(reference.f_frsize),
           "Capacity must match statvfs scaled by f_frsize");
    expect(queried->free_bytes ==
               static_cast<std::uint64_t>(reference.f_bfree) *
                   static_cast<std::uint64_t>(reference.f_frsize),
           "Free capacity must match statvfs scaled by f_frsize");
    expect(queried->available_bytes ==
               static_cast<std::uint64_t>(reference.f_bavail) *
                   static_cast<std::uint64_t>(reference.f_frsize),
           "Available capacity must match statvfs scaled by f_frsize");
    expect(queried->block_size_bytes ==
               static_cast<std::uint64_t>(reference.f_frsize),
           "The block size must be the fundamental f_frsize value");
    expect(queried->read_only == ((reference.f_flag & ST_RDONLY) != 0U),
           "Read-only state must mirror the ST_RDONLY flag");
    expect(queried->free_bytes <= queried->capacity_bytes &&
               queried->available_bytes <= queried->capacity_bytes,
           "Reported subsets never exceed reported capacity");

    const auto relative = syscape::filesystem::space(".");
    expect(relative.has_value(),
           "A relative path resolves against the current working directory");

    const auto missing =
        syscape::filesystem::space("/definitely-not-present-4711");
    expect(!missing && missing.error() ==
                           std::error_code(ENOENT, std::generic_category()),
           "A missing path preserves its native ENOENT error");

    const auto denied_probe = syscape::filesystem::space("");
    expect(!denied_probe &&
               denied_probe.error() ==
                   syscape::make_error_code(syscape::errc::invalid_argument),
           "An empty path is an invalid argument");

    const auto undecodable = syscape::filesystem::space(
        std::string("/tmp/\xffx", 7U));
    expect(!undecodable &&
               undecodable.error() ==
                   syscape::make_error_code(syscape::errc::invalid_encoding),
           "A non-UTF-8 path must fail at the public boundary");

    const auto embedded_null = syscape::filesystem::space(
        std::string("/tmp\0/ignored", 13U));
    expect(!embedded_null &&
               embedded_null.error() ==
                   syscape::make_error_code(syscape::errc::invalid_argument),
           "An embedded null must not truncate the path passed to statvfs");
}

void expect_rejected_input(
    const syscape::result<syscape::filesystem::path_length_limit>& value,
    syscape::errc expected, const char* message) {
    expect(!value && value.error() == syscape::make_error_code(expected),
           message);
}

void test_runtime_path_limits() {
    constexpr const char* probe = "/tmp";

    errno = 0;
    const long name_reference = ::pathconf(probe, _PC_NAME_MAX);
    const int name_errno = name_reference == -1 ? errno : 0;
    const auto component =
        syscape::filesystem::max_component_length(probe);
    if (!component) {
        expect(false, "A live component-length query on /tmp must "
                      "succeed on Linux");
        return;
    }
    if (name_reference == -1 && name_errno == 0) {
        expect(component->indeterminate,
               "An indeterminate reference limit must be reported "
               "through the explicit flag");
    } else {
        expect(!component->indeterminate &&
                   component->length ==
                       static_cast<std::uint64_t>(name_reference),
               "The component bound must match an independent pathconf "
               "record");
    }

    errno = 0;
    const long path_reference = ::pathconf(probe, _PC_PATH_MAX);
    const int path_errno = path_reference == -1 ? errno : 0;
    const auto whole_path = syscape::filesystem::max_path_length(probe);
    if (!whole_path) {
        expect(false,
               "A live complete-path query on /tmp must succeed on Linux");
        return;
    }
    if (path_reference == -1 && path_errno == 0) {
        expect(whole_path->indeterminate,
               "An indeterminate reference path bound must reach the "
               "explicit flag");
    } else {
        expect(!whole_path->indeterminate &&
                   whole_path->length ==
                       static_cast<std::uint64_t>(path_reference),
               "The complete-path bound must match an independent "
               "pathconf record");
    }

    const auto missing =
        syscape::filesystem::max_component_length(
            "/definitely-not-present-4711");
    expect(!missing &&
               missing.error() ==
                   std::error_code(ENOENT, std::generic_category()),
           "A missing path preserves its native ENOENT error");

    // Whether a platform's limit interface validates the whole path
    // before answering differs by resource and implementation, so the
    // reference outcome is compared rather than assumed.
    errno = 0;
    const long missing_reference =
        ::pathconf("/definitely-not-present-4711", _PC_PATH_MAX);
    const int missing_errno = missing_reference == -1 ? errno : 0;
    const auto missing_whole =
        syscape::filesystem::max_path_length(
            "/definitely-not-present-4711");
    if (missing_reference == -1 && missing_errno != 0) {
        expect(!missing_whole &&
                   missing_whole.error() ==
                       std::error_code(missing_errno,
                                       std::generic_category()),
               "A failing reference record matches the query outcome");
    } else if (missing_reference == -1) {
        expect(missing_whole && missing_whole->indeterminate,
               "An indeterminate reference record reaches the explicit "
               "flag even when the path does not exist");
    } else {
        expect(missing_whole && !missing_whole->indeterminate &&
                   missing_whole->length ==
                       static_cast<std::uint64_t>(missing_reference),
               "A reference record answered without path validation "
               "matches the query value");
    }

    expect_rejected_input(syscape::filesystem::max_component_length(""),
                          syscape::errc::invalid_argument,
                          "An empty path is an invalid argument for the "
                          "component query");
    expect_rejected_input(
        syscape::filesystem::max_component_length(std::string("/a\xff", 3U)),
        syscape::errc::invalid_encoding,
        "A non-UTF-8 path fails at the public boundary for the "
        "component query");
    expect_rejected_input(
        syscape::filesystem::max_path_length(std::string("/a\0b", 5U)),
        syscape::errc::invalid_argument,
        "An embedded null is an invalid argument for the whole-path "
        "query");
}

void test_runtime_volume_id() {
    constexpr const char* probe = "/tmp";

    struct ::statfs reference {};
    expect(::statfs(probe, &reference) == 0,
           "The statfs reference lookup must not fail natively");
    std::uint32_t first = 0U;
    std::uint32_t second = 0U;
    ::std::memcpy(&first, &reference.f_fsid.__val[0], sizeof(first));
    ::std::memcpy(&second, &reference.f_fsid.__val[1], sizeof(second));

    const auto queried = syscape::filesystem::volume_id(probe);
    if (!queried) {
        expect(false, "A live volume-identifier query must succeed on a "
                      "mounted volume");
        return;
    }
    expect(queried->size() == 16U,
           "The identifier renders at the documented fixed width of "
           "sixteen digits");
    bool hexadecimal_only = true;
    for (const char digit : *queried) {
        const bool lowercase_hex =
            (digit >= '0' && digit <= '9') ||
            (digit >= 'a' && digit <= 'f');
        hexadecimal_only = hexadecimal_only && lowercase_hex;
    }
    expect(hexadecimal_only,
           "Every rendered digit is a lowercase hexadecimal digit");
    expect(*queried == syscape::detail::filesystem_common::
                           render_hex_word_pair(first, second),
           "The rendering must match the recorded statfs word pair in "
           "documented order");

    const auto again = syscape::filesystem::volume_id(probe);
    expect(again && *again == *queried,
           "Two queries of one mounted volume agree on its identifier");

    const auto pseudo =
        syscape::filesystem::volume_id("/proc");
    expect(pseudo && pseudo->size() == 16U,
           "A pseudofilesystem without a distinguishing identifier is "
           "valid data rendered verbatim");

    const auto missing =
        syscape::filesystem::volume_id("/definitely-not-present-4711");
    expect(!missing &&
               missing.error() ==
                   std::error_code(ENOENT, std::generic_category()),
           "A missing path preserves its native ENOENT error");

    const auto empty_probe = syscape::filesystem::volume_id("");
    expect(!empty_probe &&
               empty_probe.error() ==
                   syscape::make_error_code(syscape::errc::invalid_argument),
           "An empty path is an invalid argument for the identifier "
           "query");

    const auto undecodable =
        syscape::filesystem::volume_id(std::string("/tmp/\xffx", 7U));
    expect(!undecodable &&
               undecodable.error() ==
                   syscape::make_error_code(syscape::errc::invalid_encoding),
           "A non-UTF-8 path fails at the public boundary for the "
           "identifier query");
}

} // namespace

int main() {
    test_parse_basic_records();
    test_parse_escapes();
    test_parse_malformed_records();
    test_space_snapshot_computation();
    test_pathconf_conversion();
    test_hex_rendering();
    test_common_validation();
    test_runtime_mounts();
    test_runtime_space();
    test_runtime_path_limits();
    test_runtime_volume_id();
    return failures == 0 ? 0 : 1;
}
