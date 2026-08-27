#ifndef SYSCAPE_DETAIL_STORAGE_LINUX_HPP
#define SYSCAPE_DETAIL_STORAGE_LINUX_HPP

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <charconv>
#include <dirent.h>
#include <limits>
#include <sys/stat.h>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <syscape/detail/linux/directory.hpp>
#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/storage/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace storage_backend {

/// Root of the kernel's block-device class interface.
///
/// The attributes read here follow Documentation/ABI/testing/sysfs-block and
/// Documentation/ABI/testing/sysfs-queue, which the kernel documents but has
/// not promoted to its stable ABI classification, so future kernels may
/// evolve the rendered values. Parsing therefore stays strict: recognized
/// attributes with undocumented renderings fail honestly instead of being
/// guessed.
constexpr const char* block_root = "/sys/block/";

/// Number of bytes in one kernel-recorded size unit. The kernel documents
/// the size attribute in fixed 512-byte sectors regardless of the logical
/// block size.
constexpr std::uint64_t size_unit_bytes = 512U;

/// Trims the whitespace that a single sysfs attribute read carries around
/// its value, including the space padding firmware strings use.
inline std::string_view trim_attribute(std::string_view value) noexcept {
    const auto blank = [](char letter) noexcept {
        return letter == ' ' || letter == '\t' || letter == '\r' ||
               letter == '\n';
    };
    while (!value.empty() && blank(value.front())) { value.remove_prefix(1U); }
    while (!value.empty() && blank(value.back())) { value.remove_suffix(1U); }
    return value;
}

/// Parses a plain nonnegative integer rendering shared by several
/// attributes.
///
/// Sysfs renders numbers without suffixes or signs, so any trailing or
/// leading text is malformed platform data. Renderings beyond the output
/// range report value_too_large so callers can distinguish unusable data
/// from unusable arithmetic.
template <typename Integer>
inline result<Integer> parse_number(std::string_view input) {
    const std::string_view value = trim_attribute(input);
    if (value.empty()) { return fail(errc::malformed_data); }
    constexpr std::uint64_t limit =
        static_cast<std::uint64_t>(std::numeric_limits<Integer>::max());
    std::uint64_t widened = 0U;
    const char* first = value.data();
    const char* last = first + value.size();
    const std::from_chars_result parsed =
        std::from_chars(first, last, widened);
    if (parsed.ec == std::errc::result_out_of_range ||
        (parsed.ec == std::errc() && parsed.ptr == last &&
         widened > limit)) {
        return fail(errc::value_too_large);
    }
    if (parsed.ec != std::errc() || parsed.ptr != last) {
        return fail(errc::malformed_data);
    }
    return static_cast<Integer>(widened);
}

/// Parses the documented zero-or-one Boolean renderings shared by the
/// removable and rotational attributes.
inline result<bool> parse_flag(std::string_view input) {
    const std::string_view value = trim_attribute(input);
    if (value == "0") { return false; }
    if (value == "1") { return true; }
    return fail(errc::malformed_data);
}

/// Maps the kernel-recorded backing-device subsystem onto the portable
/// transport vocabulary.
///
/// The kernel assigns whole disks to the subsystem of their backing device,
/// which names the transport layer rather than the controller protocol: a
/// SATA or USB disk behind the SCSI stack records scsi, so this mapping
/// cannot distinguish those transports and callers must not compare Linux
/// classifications against other platforms' protocol renderings. Subsystems
/// outside the vocabulary, including virtio, record unknown because
/// guessing a nearest value would fabricate structure the platform does not
/// expose.
inline storage_common::bus_classification classify_subsystem(
    std::string_view subsystem) noexcept {
    using storage_common::bus_classification;
    if (subsystem == "scsi") { return bus_classification::scsi; }
    if (subsystem == "nvme") { return bus_classification::nvme; }
    if (subsystem == "mmc") { return bus_classification::mmc; }
    return bus_classification::unknown;
}

/// Confirms that a backing-device subsystem survived an attribute walk
/// without disappearing or being replaced.
inline bool backing_subsystem_unchanged(
    std::string_view initial, bool final_resolved,
    std::string_view final_value) noexcept {
    return final_resolved && initial == final_value;
}

/// One sysfs attribute read together with its presence.
///
/// Drives legitimately omit individual attributes, so a missing file records
/// an absent field while every other native failure propagates.
struct attribute_read {
    /// Whether the attribute file existed during the call.
    bool exists = false;
    /// The raw file content, meaningful only when exists is true.
    std::string content;
};

/// Reads one attribute of one block-device directory entry.
inline result<attribute_read> read_optional_attribute(
    const std::string& entry, const char* attribute) {
    const std::string path =
        std::string(block_root) + entry + "/" + attribute;
    const result<std::string> content =
        linux_platform::read_text_file(path.c_str());
    if (!content) {
        if (content.error() ==
            std::error_code(ENOENT, std::generic_category())) {
            return attribute_read{};
        }
        return fail(content.error());
    }
    return attribute_read{true, std::move(*content)};
}

/// Copies a present text attribute when its trimmed rendering stays
/// nonempty.
///
/// Firmware strings are frequently blank padding, and recording an empty
/// vendor or model would present absence as data, so a wholly blank
/// rendering records an absent field instead.
inline void apply_text(const attribute_read& attribute, bool& present,
                       std::string& destination) {
    if (!attribute.exists) { return; }
    const std::string_view trimmed =
        trim_attribute(std::string_view(attribute.content));
    if (trimmed.empty()) { return; }
    present = true;
    destination.assign(trimmed.begin(), trimmed.end());
}

/// Reads the final component of one symbolic link inside the sysfs tree.
///
/// A link whose target vanishes between listing and resolution is part of
/// an expected reconfiguration race and reports an empty view rather than a
/// native failure; every other failure propagates unchanged.
inline result<std::string> read_link_basename(const std::string& path,
                                              bool& resolved) {
    resolved = false;
    std::vector<char> buffer(256U);
    for (;;) {
        const ssize_t length =
            ::readlink(path.c_str(), buffer.data(), buffer.size());
        if (length < 0) {
            const std::error_code error(errno, std::generic_category());
            if (error == std::error_code(ENOENT, std::generic_category())) {
                return {};
            }
            if (error != std::error_code(ERANGE, std::generic_category())) {
                return fail(error);
            }
        } else if (static_cast<std::size_t>(length) < buffer.size()) {
            // POSIX permits readlink to place a truncated copy without
            // reporting an error, so only a result strictly shorter than
            // the buffer proves completion.
            const std::string_view target(buffer.data(),
                                          static_cast<std::size_t>(length));
            const std::size_t base = target.rfind('/');
            resolved = true;
            if (base == std::string_view::npos) {
                return std::string(target);
            }
            return std::string(target.substr(base + 1U));
        }
        if (buffer.size() > 4096U) {
            return fail(std::error_code(EOVERFLOW, std::generic_category()));
        }
        buffer.resize(buffer.size() * 2U);
    }
}

/// Converts the kernel-recorded sector count into bytes with an explicit
/// overflow rejection instead of silent truncation.
inline result<std::uint64_t> capacity_in_bytes(std::uint64_t sectors) {
    if (sectors >
        std::numeric_limits<std::uint64_t>::max() / size_unit_bytes) {
        return fail(errc::value_too_large);
    }
    return sectors * size_unit_bytes;
}

/// One assembled drive snapshot together with its presence.
struct drive_snapshot {
    /// Whether the entry described a hardware-backed whole disk during the
    /// call.
    bool recorded = false;
    /// The assembled record, meaningful only when recorded is true.
    storage_common::drive_record record;
};

/// Collects the recorded attributes of one top-level block-device entry.
///
/// Entries without a backing device node describe software constructs such
/// as loop, device-mapper, memory-backed, and compressed-RAM devices rather
/// than recorded drives, so they produce an unrecorded snapshot. Entries
/// whose backing node disappears between listing and reading are expected
/// removal races and likewise stay unrecorded; every other failure
/// propagates unchanged so permission and input problems can never
/// masquerade as a complete enumeration.
inline result<drive_snapshot> collect_drive(const char* name) {
    const std::string entry(name);

    // The device symbolic link distinguishes hardware-backed disks from
    // software constructs: real controllers create it, while loop,
    // device-mapper, zram, and RAM devices do not. Resolving it also proves
    // the entry still existed during the call.
    const std::string device_path =
        std::string(block_root) + entry + "/device";
    struct stat backing;
    if (::lstat(device_path.c_str(), &backing) != 0) {
        const std::error_code error(errno, std::generic_category());
        if (error == std::error_code(ENOENT, std::generic_category())) {
            return drive_snapshot{};
        }
        return fail(error);
    }

    bool subsystem_resolved = false;
    const result<std::string> subsystem = read_link_basename(
        device_path + "/subsystem", subsystem_resolved);
    if (!subsystem) { return fail(subsystem.error()); }
    if (!subsystem_resolved) {
        // The backing target vanished after lstat. Treat the entry as an
        // expected hot-removal race rather than returning a skeletal drive.
        return drive_snapshot{};
    }

    storage_common::drive_record record;
    record.identifier = entry;
    record.bus = classify_subsystem(*subsystem);

    const result<attribute_read> size_attribute =
        read_optional_attribute(entry, "size");
    if (!size_attribute) { return fail(size_attribute.error()); }
    if (size_attribute->exists) {
        const result<std::uint64_t> sectors =
            parse_number<std::uint64_t>(size_attribute->content);
        if (!sectors) { return fail(sectors.error()); }
        const result<std::uint64_t> bytes = capacity_in_bytes(*sectors);
        if (!bytes) { return fail(bytes.error()); }
        record.has_capacity_bytes = true;
        record.capacity_bytes = *bytes;
    }

    const result<attribute_read> logical_attribute =
        read_optional_attribute(entry, "queue/logical_block_size");
    if (!logical_attribute) { return fail(logical_attribute.error()); }
    if (logical_attribute->exists) {
        const result<std::uint32_t> parsed =
            parse_number<std::uint32_t>(logical_attribute->content);
        if (!parsed) { return fail(parsed.error()); }
        record.has_logical_sector_size_bytes = true;
        record.logical_sector_size_bytes = *parsed;
    }

    const result<attribute_read> physical_attribute =
        read_optional_attribute(entry, "queue/physical_block_size");
    if (!physical_attribute) { return fail(physical_attribute.error()); }
    if (physical_attribute->exists) {
        const result<std::uint32_t> parsed =
            parse_number<std::uint32_t>(physical_attribute->content);
        if (!parsed) { return fail(parsed.error()); }
        record.has_physical_sector_size_bytes = true;
        record.physical_sector_size_bytes = *parsed;
    }

    const result<attribute_read> rotational_attribute =
        read_optional_attribute(entry, "queue/rotational");
    if (!rotational_attribute) { return fail(rotational_attribute.error()); }
    if (rotational_attribute->exists) {
        const result<bool> rotational = parse_flag(rotational_attribute->content);
        if (!rotational) { return fail(rotational.error()); }
        record.has_rotational = true;
        record.rotational = *rotational;
    }

    const result<attribute_read> removable_attribute =
        read_optional_attribute(entry, "removable");
    if (!removable_attribute) { return fail(removable_attribute.error()); }
    if (removable_attribute->exists) {
        const result<bool> removable = parse_flag(removable_attribute->content);
        if (!removable) { return fail(removable.error()); }
        record.removable = *removable;
    }

    const result<attribute_read> vendor_attribute =
        read_optional_attribute(entry, "device/vendor");
    if (!vendor_attribute) { return fail(vendor_attribute.error()); }
    apply_text(*vendor_attribute, record.has_vendor, record.vendor);

    const result<attribute_read> model_attribute =
        read_optional_attribute(entry, "device/model");
    if (!model_attribute) { return fail(model_attribute.error()); }
    apply_text(*model_attribute, record.has_model, record.model);

    const result<attribute_read> revision_attribute =
        read_optional_attribute(entry, "device/rev");
    if (!revision_attribute) { return fail(revision_attribute.error()); }
    apply_text(*revision_attribute,
               record.has_firmware_revision, record.firmware_revision);

    // Confirm that the backing device survived the complete attribute walk.
    // Comparing the subsystem also rejects a remove-and-replace race that
    // reused the same top-level block name for a different device.
    bool final_subsystem_resolved = false;
    const result<std::string> final_subsystem = read_link_basename(
        device_path + "/subsystem", final_subsystem_resolved);
    if (!final_subsystem) { return fail(final_subsystem.error()); }
    if (!backing_subsystem_unchanged(
            *subsystem, final_subsystem_resolved, *final_subsystem)) {
        return drive_snapshot{};
    }

    drive_snapshot snapshot;
    snapshot.recorded = true;
    snapshot.record = std::move(record);
    return snapshot;
}

/// Walks every visible top-level block-device entry.
///
/// A system whose kernel exposes no block class enumerates zero devices
/// instead of failing, because the absence of drives is ordinary on
/// diskless network clients. All other directory failures propagate.
template <typename Visit>
inline result<void> walk_block_devices(Visit visit) {
    linux_platform::directory_handle directory(block_root);
    if (!directory.valid()) {
        if (directory.error() == ENOENT) {
            return {};
        }
        return fail(std::error_code(directory.error(), std::generic_category()));
    }
    for (;;) {
        errno = 0;
        const ::dirent* entry = ::readdir(directory.get());
        if (entry == nullptr) { break; }
        if (entry->d_name[0] == '.') { continue; }
        const result<void> outcome = visit(entry->d_name);
        if (!outcome) { return fail(outcome.error()); }
    }
    if (errno != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    return {};
}

/// Enumerates the hardware-backed whole-disk devices recorded by the
/// kernel.
///
/// Entries are ordered by ascending identifier so callers observe one
/// deterministic order for an unchanged population. The snapshot reflects
/// the devices present during the call; hot-plugged drives become visible
/// only to later calls, and every returned value can change as media are
/// loaded or removed.
inline result<std::vector<storage_common::drive_record>> drives() {
    std::vector<storage_common::drive_record> records;
    const result<void> walked = walk_block_devices(
        [&records](const char* name) -> result<void> {
            const result<drive_snapshot> snapshot = collect_drive(name);
            if (!snapshot) { return fail(snapshot.error()); }
            if (snapshot->recorded) { records.push_back(snapshot->record); }
            return {};
        });
    if (!walked) { return fail(walked.error()); }
    std::sort(records.begin(), records.end(),
              [](const storage_common::drive_record& left,
                 const storage_common::drive_record& right) {
                  return left.identifier < right.identifier;
              });
    return records;
}

/// Classifies the partition scheme from the partition's UUID format.
inline storage_common::partition_scheme_classification
classify_partition_scheme(std::string_view uuid) noexcept {
    using storage_common::partition_scheme_classification;
    const auto is_hex_char = [](char c) noexcept {
        return (c >= '0' && c <= '9') ||
               (c >= 'a' && c <= 'f') ||
               (c >= 'A' && c <= 'F');
    };
    if (uuid.size() == 36U &&
        uuid[8U] == '-' && uuid[13U] == '-' &&
        uuid[18U] == '-' && uuid[23U] == '-') {
        bool valid = true;
        for (std::size_t i = 0U; i < 36U; ++i) {
            if (i == 8U || i == 13U || i == 18U || i == 23U) { continue; }
            if (!is_hex_char(uuid[i])) { valid = false; break; }
        }
        if (valid) {
            return partition_scheme_classification::gpt;
        }
    }
    const std::size_t hyphen = uuid.find('-');
    if (hyphen == 8U && uuid.size() >= 10U && uuid.size() <= 12U) {
        bool valid = true;
        for (std::size_t i = 0U; i < 8U; ++i) {
            if (!is_hex_char(uuid[i])) { valid = false; break; }
        }
        for (std::size_t i = 9U; i < uuid.size(); ++i) {
            if (uuid[i] < '0' || uuid[i] > '9') { valid = false; break; }
        }
        if (valid) {
            return partition_scheme_classification::mbr;
        }
    }
    return partition_scheme_classification::unknown;
}

/// Returns the octal digit value of a character, or -1 for non-octal text.
inline int octal_digit_value(char value) noexcept {
    return value >= '0' && value <= '7' ? value - '0' : -1;
}

/// Decodes one octal escape sequence of the documented /proc/mounts form.
inline result<char> decode_mount_escape(std::string_view field,
                                        std::size_t backslash) {
    if (field.size() - backslash < 4U) {
        return fail(errc::malformed_data);
    }
    const int hundreds = octal_digit_value(field[backslash + 1U]);
    const int tens = octal_digit_value(field[backslash + 2U]);
    const int ones = octal_digit_value(field[backslash + 3U]);
    if (hundreds < 0 || tens < 0 || ones < 0) {
        return fail(errc::malformed_data);
    }

    switch (hundreds * 64 + tens * 8 + ones) {
    case 040: return ' ';
    case 011: return '\t';
    case 012: return '\n';
    case 0134: return '\\';
    default: return fail(errc::malformed_data);
    }
}

/// Decodes a device, mount-point, or type field reported by the kernel.
inline result<std::string> decode_mount_field(std::string_view field) {
    std::string output;
    output.reserve(field.size());
    std::size_t offset = 0U;
    while (offset < field.size()) {
        if (field[offset] != '\\') {
            output.push_back(field[offset]);
            ++offset;
            continue;
        }
        const result<char> decoded = decode_mount_escape(field, offset);
        if (!decoded) { return fail(decoded.error()); }
        output.push_back(*decoded);
        offset += 4U;
    }
    return output;
}

/// Structure representing an entry in the system mount table.
struct mount_entry {
    std::string device_name;
    std::string mount_point;
    std::string fstype;
};

/// Parses a /proc/self/mounts snapshot into mounted block-device mappings.
inline result<std::vector<mount_entry>> parse_mount_table(
    std::string_view text) {
    std::vector<mount_entry> entries;
    std::size_t offset = 0U;
    while (offset < text.size()) {
        const std::size_t end = text.find('\n', offset);
        const std::size_t limit =
            (end == std::string_view::npos) ? text.size() : end;
        const std::string_view line = text.substr(offset, limit - offset);
        offset = (end == std::string_view::npos) ? text.size() : end + 1U;
        if (line.empty()) { continue; }
        std::size_t p = 0U;
        while (p < line.size() && (line[p] == ' ' || line[p] == '\t')) { ++p; }
        const std::size_t s1 = p;
        while (p < line.size() && line[p] != ' ' && line[p] != '\t') { ++p; }
        const std::string_view raw_source = line.substr(s1, p - s1);
        while (p < line.size() && (line[p] == ' ' || line[p] == '\t')) { ++p; }
        const std::size_t s2 = p;
        while (p < line.size() && line[p] != ' ' && line[p] != '\t') { ++p; }
        const std::string_view raw_target = line.substr(s2, p - s2);
        while (p < line.size() && (line[p] == ' ' || line[p] == '\t')) { ++p; }
        const std::size_t s3 = p;
        while (p < line.size() && line[p] != ' ' && line[p] != '\t') { ++p; }
        const std::string_view raw_fstype = line.substr(s3, p - s3);

        if (raw_source.empty() || raw_target.empty() || raw_fstype.empty()) {
            return fail(errc::malformed_data);
        }
        std::size_t field_count = 3U;
        while (p < line.size()) {
            while (p < line.size() &&
                   (line[p] == ' ' || line[p] == '\t')) {
                ++p;
            }
            if (p == line.size()) { break; }
            ++field_count;
            while (p < line.size() && line[p] != ' ' && line[p] != '\t') {
                ++p;
            }
        }
        if (field_count < 6U) { return fail(errc::malformed_data); }

        if (raw_source.rfind("/dev/", 0U) == 0U && raw_source.size() > 5U &&
            !raw_target.empty()) {
            const result<std::string> decoded_source =
                decode_mount_field(raw_source.substr(5U));
            if (!decoded_source) { return fail(decoded_source.error()); }
            const result<std::string> decoded_target =
                decode_mount_field(raw_target);
            if (!decoded_target) { return fail(decoded_target.error()); }
            const result<std::string> decoded_fstype =
                decode_mount_field(raw_fstype);
            if (!decoded_fstype) { return fail(decoded_fstype.error()); }

            mount_entry entry;
            entry.device_name = std::move(*decoded_source);
            entry.mount_point = std::move(*decoded_target);
            entry.fstype = std::move(*decoded_fstype);
            entries.push_back(std::move(entry));
        }
    }
    return entries;
}

/// Reads /proc/self/mounts and collects mounted block device mappings.
inline result<std::vector<mount_entry>> collect_mount_table() {
    const result<std::string> content =
        linux_platform::read_text_file("/proc/self/mounts");
    if (!content) { return fail(content.error()); }
    return parse_mount_table(*content);
}

/// One assembled partition snapshot together with its presence.
struct partition_snapshot {
    bool recorded = false;
    storage_common::partition_record record;
};

/// Collects attributes of one partition directory entry under a disk.
inline result<partition_snapshot> collect_partition(
    const std::string& disk_entry, const std::string& part_entry,
    const std::vector<mount_entry>& mounts) {
    const std::string part_path =
        std::string(block_root) + disk_entry + "/" + part_entry;

    const std::string partition_file = part_path + "/partition";
    const result<std::string> partn_content =
        linux_platform::read_text_file(partition_file.c_str());
    if (!partn_content) {
        if (partn_content.error() ==
                std::error_code(ENOENT, std::generic_category()) ||
            partn_content.error() ==
                std::error_code(ENOTDIR, std::generic_category())) {
            return partition_snapshot{};
        }
        return fail(partn_content.error());
    }
    const result<std::uint32_t> partn =
        parse_number<std::uint32_t>(*partn_content);
    if (!partn) { return fail(partn.error()); }

    storage_common::partition_record record;
    record.identifier = part_entry;
    record.disk_identifier = disk_entry;
    record.partition_number = *partn;

    const std::string start_file = part_path + "/start";
    const result<std::string> start_content =
        linux_platform::read_text_file(start_file.c_str());
    if (start_content) {
        const result<std::uint64_t> start_sectors =
            parse_number<std::uint64_t>(*start_content);
        if (!start_sectors) { return fail(start_sectors.error()); }
        const result<std::uint64_t> start_bytes =
            capacity_in_bytes(*start_sectors);
        if (!start_bytes) { return fail(start_bytes.error()); }
        record.has_start_offset_bytes = true;
        record.start_offset_bytes = *start_bytes;
    } else if (start_content.error() !=
                   std::error_code(ENOENT, std::generic_category())) {
        return fail(start_content.error());
    }

    const std::string size_file = part_path + "/size";
    const result<std::string> size_content =
        linux_platform::read_text_file(size_file.c_str());
    if (size_content) {
        const result<std::uint64_t> size_sectors =
            parse_number<std::uint64_t>(*size_content);
        if (!size_sectors) { return fail(size_sectors.error()); }
        const result<std::uint64_t> size_bytes =
            capacity_in_bytes(*size_sectors);
        if (!size_bytes) { return fail(size_bytes.error()); }
        record.has_size_bytes = true;
        record.size_bytes = *size_bytes;
    } else if (size_content.error() !=
                   std::error_code(ENOENT, std::generic_category())) {
        return fail(size_content.error());
    }

    const std::string ro_file = part_path + "/ro";
    const result<std::string> ro_content =
        linux_platform::read_text_file(ro_file.c_str());
    if (ro_content) {
        const result<bool> ro_flag = parse_flag(*ro_content);
        if (!ro_flag) { return fail(ro_flag.error()); }
        record.is_read_only = *ro_flag;
    } else if (ro_content.error() !=
                   std::error_code(ENOENT, std::generic_category())) {
        return fail(ro_content.error());
    }

    const std::string uevent_file = part_path + "/uevent";
    const result<std::string> uevent_content =
        linux_platform::read_text_file(uevent_file.c_str());
    if (uevent_content) {
        const std::string_view uevent_text(*uevent_content);
        std::size_t offset = 0U;
        while (offset < uevent_text.size()) {
            const std::size_t end = uevent_text.find('\n', offset);
            const std::size_t limit =
                (end == std::string_view::npos) ? uevent_text.size() : end;
            const std::string_view line =
                uevent_text.substr(offset, limit - offset);
            offset = (end == std::string_view::npos)
                         ? uevent_text.size()
                         : end + 1U;
            const std::size_t eq = line.find('=');
            if (eq == std::string_view::npos) { continue; }
            const std::string_view key = line.substr(0U, eq);
            const std::string_view val =
                trim_attribute(line.substr(eq + 1U));
            if (key == "PARTNAME" && !val.empty()) {
                record.has_name = true;
                record.name = std::string(val);
            } else if (key == "PARTUUID" && !val.empty()) {
                record.has_uuid = true;
                record.uuid = std::string(val);
            }
        }
    } else if (uevent_content.error() !=
                   std::error_code(ENOENT, std::generic_category())) {
        return fail(uevent_content.error());
    }

    record.scheme = classify_partition_scheme(
        record.has_uuid ? std::string_view(record.uuid) : std::string_view());

    for (const mount_entry& m : mounts) {
        if (m.device_name == part_entry) {
            record.is_mounted = true;
            record.mount_point = m.mount_point;
            if (!m.fstype.empty()) {
                record.has_filesystem_type = true;
                record.filesystem_type = m.fstype;
            }
            break;
        }
    }

    partition_snapshot snapshot;
    snapshot.recorded = true;
    snapshot.record = std::move(record);
    return snapshot;
}

/// Enumerates all partitions across all hardware-backed disks.
inline result<std::vector<storage_common::partition_record>> partitions() {
    std::vector<storage_common::partition_record> records;
    const result<std::vector<mount_entry>> mounts = collect_mount_table();
    if (!mounts) { return fail(mounts.error()); }

    const result<void> walked = walk_block_devices(
        [&records, &mounts](const char* disk_name) -> result<void> {
            const result<drive_snapshot> drive = collect_drive(disk_name);
            if (!drive) { return fail(drive.error()); }
            if (!drive->recorded) { return {}; }

            const std::string disk_path =
                std::string(block_root) + disk_name;
            linux_platform::directory_handle disk_dir(disk_path.c_str());
            if (!disk_dir.valid()) {
                if (disk_dir.error() == ENOENT) { return {}; }
                return fail(std::error_code(disk_dir.error(), std::generic_category()));
            }

            for (;;) {
                errno = 0;
                const ::dirent* entry = ::readdir(disk_dir.get());
                if (entry == nullptr) {
                    if (errno != 0) {
                        return fail(std::error_code(errno, std::generic_category()));
                    }
                    break;
                }
                if (entry->d_name[0] == '.') { continue; }
                const result<partition_snapshot> part =
                    collect_partition(disk_name, entry->d_name, *mounts);
                if (!part) { return fail(part.error()); }
                if (part->recorded) {
                    records.push_back(std::move(part->record));
                }
            }
            return {};
        });
    if (!walked) { return fail(walked.error()); }
    std::sort(records.begin(), records.end(),
              [](const storage_common::partition_record& left,
                 const storage_common::partition_record& right) {
                  return left.identifier < right.identifier;
              });
    return records;
}

} // namespace storage_backend
} // namespace detail
} // namespace syscape

#endif
