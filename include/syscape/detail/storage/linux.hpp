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
        const std::error_code error(errno, std::generic_category());
        if (error == std::error_code(ENOENT, std::generic_category())) {
            return {};
        }
        return fail(error);
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

} // namespace storage_backend
} // namespace detail
} // namespace syscape

#endif
