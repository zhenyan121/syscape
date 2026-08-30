#ifndef SYSCAPE_DETAIL_STORAGE_MACOS_HPP
#define SYSCAPE_DETAIL_STORAGE_MACOS_HPP

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>

#include <syscape/detail/storage/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace storage_backend {

/// Owns one CoreFoundation object reference for the duration of a query.
class cf_object {
public:
    explicit cf_object(::CFTypeRef value) noexcept : value_(value) {}
    cf_object(const cf_object&) = delete;
    cf_object& operator=(const cf_object&) = delete;
    ~cf_object() {
        if (value_ != nullptr) { ::CFRelease(value_); }
    }

    /// Returns the owned reference.
    ::CFTypeRef get() const noexcept { return value_; }

private:
    ::CFTypeRef value_;
};

/// Owns one IOKit object reference for the duration of its use.
class io_object {
public:
    explicit io_object(::io_object_t value) noexcept : value_(value) {}
    io_object(const io_object&) = delete;
    io_object& operator=(const io_object&) = delete;
    ~io_object() {
        if (value_ != 0) { static_cast<void>(::IOObjectRelease(value_)); }
    }

    ::io_object_t get() const noexcept { return value_; }

private:
    ::io_object_t value_;
};

/// Copies one CoreFoundation string into UTF-8 storage.
///
/// A string that cannot be rendered as UTF-8 reports a conversion failure
/// instead of corrupted text.
inline result<std::string> copy_utf8_string(::CFStringRef value) {
    if (value == nullptr) { return fail(errc::io_error); }
    const ::CFIndex length = ::CFStringGetLength(value);
    if (length == 0) { return std::string(); }
    const ::CFIndex maximum = ::CFStringGetMaximumSizeForEncoding(
        length, ::kCFStringEncodingUTF8);
    if (maximum <= 0) { return fail(errc::io_error); }
    std::string output;
    output.resize(static_cast<std::size_t>(maximum));
    ::CFIndex used = 0;
    const ::CFIndex converted = ::CFStringGetBytes(
        value, ::CFRangeMake(0, length), ::kCFStringEncodingUTF8, 0U,
        false, reinterpret_cast<UInt8*>(&output[0]), maximum, &used);
    if (converted != length || used < 0 || used > maximum) {
        return fail(errc::invalid_encoding);
    }
    output.resize(static_cast<std::size_t>(used));
    return output;
}

/// Copies one recorded string field out of a media-facts dictionary.
///
/// An absent key records an absent field. A key whose value is not a
/// string contradicts the platform's own schema and is malformed platform
/// data.
inline result<bool> copy_optional_string(::CFDictionaryRef facts,
                                         ::CFStringRef key, bool& present,
                                         std::string& destination) {
    const void* raw = ::CFDictionaryGetValue(facts, key);
    if (raw == nullptr) { return false; }
    if (::CFGetTypeID(raw) != ::CFStringGetTypeID()) {
        return fail(errc::malformed_data);
    }
    result<std::string> text =
        copy_utf8_string(static_cast<::CFStringRef>(raw));
    if (!text) { return fail(text.error()); }
    if (text->empty()) { return false; }
    present = true;
    destination = std::move(*text);
    return true;
}

/// Copies one recorded unsigned number out of a media-facts dictionary.
///
/// An absent key reports absence through the Boolean output; a wrong-typed
/// or negative rendering is malformed platform data because sizes cannot
/// be negative.
inline result<bool> copy_optional_number(::CFDictionaryRef facts,
                                         ::CFStringRef key,
                                         std::uint64_t& destination) {
    const void* raw = ::CFDictionaryGetValue(facts, key);
    if (raw == nullptr) { return false; }
    if (::CFGetTypeID(raw) != ::CFNumberGetTypeID()) {
        return fail(errc::malformed_data);
    }
    long long recorded = 0;
    if (!::CFNumberGetValue(static_cast<::CFNumberRef>(raw),
                            ::kCFNumberLongLongType, &recorded)) {
        return fail(errc::malformed_data);
    }
    if (recorded < 0) { return fail(errc::malformed_data); }
    destination = static_cast<std::uint64_t>(recorded);
    return true;
}

/// Copies one recorded Boolean fact out of a media-facts dictionary.
///
/// An absent key records false because IOMedia publishes its ejectability
/// with an explicit default, so absence and an explicit no carry the same
/// meaning; any present non-Boolean rendering is malformed platform data.
inline result<bool> copy_optional_boolean(::CFDictionaryRef facts,
                                          ::CFStringRef key,
                                          bool& destination) {
    const void* raw = ::CFDictionaryGetValue(facts, key);
    if (raw == nullptr) { return false; }
    if (::CFGetTypeID(raw) != ::CFBooleanGetTypeID()) {
        return fail(errc::malformed_data);
    }
    destination = raw == ::kCFBooleanTrue;
    return true;
}

/// Copies one recorded UUID field out of a media-facts dictionary, handling
/// both CFUUID and CFString values.
inline result<bool> copy_optional_uuid(::CFDictionaryRef facts,
                                       ::CFStringRef key, bool& present,
                                       std::string& destination) {
    const void* raw = ::CFDictionaryGetValue(facts, key);
    if (raw == nullptr) {
        present = false;
        destination.clear();
        return false;
    }
    const ::CFTypeID type_id = ::CFGetTypeID(raw);
    if (type_id == ::CFUUIDGetTypeID()) {
        const ::CFStringRef uuid_str = ::CFUUIDCreateString(
            ::kCFAllocatorDefault, static_cast<::CFUUIDRef>(raw));
        if (uuid_str == nullptr) { return fail(errc::resource_exhausted); }
        const cf_object owned(uuid_str);
        result<std::string> text = copy_utf8_string(uuid_str);
        if (!text) { return fail(text.error()); }
        if (text->empty()) {
            present = false;
            destination.clear();
            return false;
        }
        present = true;
        destination = std::move(*text);
        return true;
    }
    if (type_id == ::CFStringGetTypeID()) {
        result<std::string> text =
            copy_utf8_string(static_cast<::CFStringRef>(raw));
        if (!text) { return fail(text.error()); }
        if (text->empty()) {
            present = false;
            destination.clear();
            return false;
        }
        present = true;
        destination = std::move(*text);
        return true;
    }
    return fail(errc::malformed_data);
}

/// Copies one recorded filesystem mount URL or path field out of a
/// media-facts dictionary, handling both CFURL and CFString values.
inline result<bool> copy_optional_url_path(::CFDictionaryRef facts,
                                           ::CFStringRef key, bool& present,
                                           std::string& destination) {
    const void* raw = ::CFDictionaryGetValue(facts, key);
    if (raw == nullptr) {
        present = false;
        destination.clear();
        return false;
    }
    const ::CFTypeID type_id = ::CFGetTypeID(raw);
    if (type_id == ::CFURLGetTypeID()) {
        const ::CFStringRef path_str = ::CFURLCopyFileSystemPath(
            static_cast<::CFURLRef>(raw), ::kCFURLPOSIXPathStyle);
        if (path_str == nullptr) {
            return fail(errc::malformed_data);
        }
        const cf_object owned(path_str);
        result<std::string> text = copy_utf8_string(path_str);
        if (!text) { return fail(text.error()); }
        if (text->empty()) {
            present = false;
            destination.clear();
            return false;
        }
        present = true;
        destination = std::move(*text);
        return true;
    }
    if (type_id == ::CFStringGetTypeID()) {
        result<std::string> text =
            copy_utf8_string(static_cast<::CFStringRef>(raw));
        if (!text) { return fail(text.error()); }
        if (text->empty()) {
            present = false;
            destination.clear();
            return false;
        }
        present = true;
        destination = std::move(*text);
        return true;
    }
    return fail(errc::malformed_data);
}

/// Maps the platform's recorded device-protocol renderings onto the
/// portable transport vocabulary.
///
/// Protocol names outside this vocabulary record unknown rather than
/// failing, because unfamiliar protocols describe unfamiliar hardware
/// honestly.
inline storage_common::bus_classification classify_protocol(
    const std::string& protocol) noexcept {
    using storage_common::bus_classification;
    if (protocol == "ATA") { return bus_classification::sata; }
    if (protocol == "ATAPI") { return bus_classification::atapi; }
    if (protocol == "USB") { return bus_classification::usb; }
    if (protocol == "FireWire") { return bus_classification::firewire; }
    if (protocol == "Secure Digital") { return bus_classification::sd; }
    if (protocol == "NVMe") { return bus_classification::nvme; }
    if (protocol == "RAID") { return bus_classification::raid; }
    return bus_classification::unknown;
}

/// The platform's rendering for image-backed media that describes a file
/// rather than a drive.
constexpr const char* virtual_protocol = "Virtual";

/// One media description reduced to plain documented values awaiting
/// boundary conversion.
struct media_facts {
    /// kIOBSDNameKey rendering.
    std::string identifier;
    /// kDADiskDescriptionDeviceProtocolKey rendering.
    std::string protocol;
    /// Whether the platform recorded the media as image-backed virtual.
    bool virtual_media = false;
    /// kIOMediaSizeKey value in bytes.
    std::uint64_t capacity_bytes = 0U;
    /// Whether the description carried kIOMediaPreferredBlockSizeKey.
    bool has_block_size = false;
    /// kIOMediaPreferredBlockSizeKey value in bytes.
    std::uint64_t block_size_bytes = 0U;
    /// kIOMediaEjectableKey value.
    bool ejectable = false;
    /// Whether the description carried a device-model rendering.
    bool has_model = false;
    /// kDADiskDescriptionDeviceModelKey rendering.
    std::string model;
    /// Whether the backing-device chain exposed a vendor rendering.
    bool has_vendor = false;
    /// Recorded vendor rendering.
    std::string vendor;
    /// Whether the backing-device chain exposed a revision rendering.
    bool has_revision = false;
    /// Recorded firmware-revision rendering.
    std::string revision;
};

/// Converts one assembled media-facts dictionary into plain values.
///
/// The BSD name and the transport protocol are structural requirements of
/// the enumeration contract, so their absence is malformed platform data.
inline result<media_facts> convert_media_dictionary(
    const void* raw_dictionary) {
    // CFArray elements arrive as untyped references; the type check at the
    // call site guarantees the dictionary conversion.
    const ::CFDictionaryRef dictionary =
        static_cast<::CFDictionaryRef>(raw_dictionary);
    media_facts facts;

    bool has_identifier = false;
    std::string identifier;
    const result<bool> copied_identifier = copy_optional_string(
        dictionary, ::kDADiskDescriptionMediaBSDNameKey, has_identifier,
        identifier);
    if (!copied_identifier) { return fail(copied_identifier.error()); }
    if (!has_identifier) { return fail(errc::malformed_data); }
    facts.identifier = std::move(identifier);

    bool has_protocol = false;
    std::string protocol;
    const result<bool> copied_protocol = copy_optional_string(
        dictionary, ::kDADiskDescriptionDeviceProtocolKey, has_protocol,
        protocol);
    if (!copied_protocol) { return fail(copied_protocol.error()); }
    if (!has_protocol) { return fail(errc::malformed_data); }
    facts.protocol = protocol;
    facts.virtual_media = protocol == virtual_protocol;

    std::uint64_t capacity = 0U;
    const result<bool> copied_capacity = copy_optional_number(
        dictionary, ::kDADiskDescriptionMediaSizeKey, capacity);
    if (!copied_capacity) { return fail(copied_capacity.error()); }
    if (!copied_capacity.value()) { return fail(errc::malformed_data); }
    facts.capacity_bytes = capacity;

    std::uint64_t block_size = 0U;
    const result<bool> copied_block_size = copy_optional_number(
        dictionary, ::kDADiskDescriptionMediaBlockSizeKey, block_size);
    if (!copied_block_size) { return fail(copied_block_size.error()); }
    if (copied_block_size.value()) {
        facts.has_block_size = true;
        facts.block_size_bytes = block_size;
    }

    const result<bool> copied_ejectable = copy_optional_boolean(
        dictionary, ::kDADiskDescriptionMediaEjectableKey,
        facts.ejectable);
    if (!copied_ejectable) { return fail(copied_ejectable.error()); }

    const result<bool> copied_model = copy_optional_string(
        dictionary, ::kDADiskDescriptionDeviceModelKey, facts.has_model,
        facts.model);
    if (!copied_model) { return fail(copied_model.error()); }

    const result<bool> copied_vendor = copy_optional_string(
        dictionary, CFSTR("Vendor"), facts.has_vendor, facts.vendor);
    if (!copied_vendor) { return fail(copied_vendor.error()); }

    const result<bool> copied_revision = copy_optional_string(
        dictionary, CFSTR("Revision"), facts.has_revision,
        facts.revision);
    if (!copied_revision) { return fail(copied_revision.error()); }

    return facts;
}

/// Platform calls used to assemble whole-disk media descriptions.
///
/// The indirection exists so tests can drive collection with synthetic
/// dictionaries instead of real disks; production callers always use the
/// native implementation.
struct native_drive_api {
    /// Returns an owned array of whole-media fact dictionaries.
    ///
    /// Each dictionary carries the BSD name, the DiskArbitration protocol
    /// rendering, the recorded sizes and ejectability, and every identity
    /// string the registry chain exposes. Image-backed media whose
    /// protocol the platform records as virtual are excluded during
    /// assembly because they describe files rather than drives.
    static result<::CFArrayRef> whole_media_facts();

    /// Returns an owned array of partition-media fact dictionaries.
    static result<::CFArrayRef> partition_media_facts();
};

/// Collects drive records from media-fact dictionaries through the given
/// API.
///
/// Entries are ordered by ascending identifier so callers observe one
/// deterministic order for an unchanged population. Descriptions whose
/// protocol the platform records as virtual stay excluded because they
/// describe files rather than drives.
template <typename DriveApi>
inline result<std::vector<storage_common::drive_record>> collect_drives() {
    const result<::CFArrayRef> collected = DriveApi::whole_media_facts();
    if (!collected) { return fail(collected.error()); }
    const cf_object owned(*collected);
    const ::CFArrayRef facts_array =
        static_cast<::CFArrayRef>(owned.get());

    std::vector<storage_common::drive_record> records;
    const ::CFIndex count = ::CFArrayGetCount(facts_array);
    for (::CFIndex index = 0; index < count; ++index) {
        const void* dictionary = ::CFArrayGetValueAtIndex(facts_array,
                                                          index);
        if (dictionary == nullptr ||
            ::CFGetTypeID(dictionary) != ::CFDictionaryGetTypeID()) {
            return fail(errc::malformed_data);
        }
        const result<media_facts> facts =
            convert_media_dictionary(dictionary);
        if (!facts) { return fail(facts.error()); }
        if (facts->virtual_media) { continue; }

        storage_common::drive_record record;
        record.identifier = facts->identifier;
        record.has_vendor = facts->has_vendor;
        record.vendor = facts->vendor;
        record.has_model = facts->has_model;
        record.model = facts->model;
        record.has_firmware_revision = facts->has_revision;
        record.firmware_revision = facts->revision;
        record.removable = facts->ejectable;
        record.bus = classify_protocol(facts->protocol);
        record.has_capacity_bytes = true;
        record.capacity_bytes = facts->capacity_bytes;
        if (facts->has_block_size &&
            facts->block_size_bytes <=
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::uint32_t>::max())) {
            record.has_logical_sector_size_bytes = true;
            record.logical_sector_size_bytes =
                static_cast<std::uint32_t>(facts->block_size_bytes);
        }
        records.push_back(std::move(record));
    }

    std::sort(records.begin(), records.end(),
              [](const storage_common::drive_record& left,
                 const storage_common::drive_record& right) {
                  return left.identifier < right.identifier;
              });
    return records;
}

/// Copies one recorded string property of one IOKit registry entry into
/// the target dictionary when it is a nonempty string and the field has no
/// usable rendering yet.
///
/// The registry places vendor, product, and revision strings on ancestors
/// of the media object rather than on the media itself, so the whole-disk
/// walk climbs its chain looking for them.
inline void record_first_string(::io_registry_entry_t entry,
                         ::CFStringRef source_key,
                         ::CFMutableDictionaryRef target,
                         ::CFStringRef target_key) {
    if (::CFDictionaryGetValue(target, target_key) != nullptr) { return; }
    const ::CFTypeRef raw = ::IORegistryEntryCreateCFProperty(
        entry, source_key, ::kCFAllocatorDefault, 0);
    if (raw == nullptr) { return; }
    const cf_object owned(raw);
    if (::CFGetTypeID(raw) != ::CFStringGetTypeID()) { return; }
    if (::CFStringGetLength(static_cast<::CFStringRef>(raw)) == 0) {
        return;
    }
    ::CFDictionarySetValue(target, target_key, raw);
}

/// Climbs the bounded backing-device chain behind one media object and
/// records the first vendor, product, and revision renderings it finds.
///
/// Product renderings back up the DiskArbitration model key, which not
/// every controller family publishes.
inline void gather_backing_identity(::io_service_t media,
                             ::CFMutableDictionaryRef target) {
    const bool complete =
        ::CFDictionaryGetValue(target, CFSTR("Vendor")) != nullptr &&
        ::CFDictionaryGetValue(
            target, ::kDADiskDescriptionDeviceModelKey) != nullptr &&
        ::CFDictionaryGetValue(target, CFSTR("Revision")) != nullptr;
    if (complete) { return; }

    ::io_object_t current = 0;
    if (::IORegistryEntryGetParentEntry(media, kIOServicePlane, &current) !=
        KERN_SUCCESS) {
        return;
    }
    constexpr int maximum_levels = 10;
    for (int level = 0; level < maximum_levels && current != 0; ++level) {
        const io_object owned_level(current);

        record_first_string(current, CFSTR("Vendor"), target,
                            CFSTR("Vendor"));
        record_first_string(current, CFSTR("Product"), target,
                            ::kDADiskDescriptionDeviceModelKey);
        record_first_string(current, CFSTR("Revision"), target,
                            CFSTR("Revision"));

        const void* smart_status = ::IORegistryEntryCreateCFProperty(
            current, CFSTR("SMART Status"), ::kCFAllocatorDefault, 0);
        if (smart_status != nullptr) {
            if (::CFGetTypeID(smart_status) == ::CFStringGetTypeID() &&
                ::CFDictionaryGetValue(target, CFSTR("SMART Status")) == nullptr) {
                ::CFDictionarySetValue(target, CFSTR("SMART Status"), smart_status);
            }
            ::CFRelease(smart_status);
        }

        const void* stats = ::IORegistryEntryCreateCFProperty(
            current, CFSTR("Statistics"), ::kCFAllocatorDefault, 0);
        if (stats != nullptr) {
            if (::CFGetTypeID(stats) == ::CFDictionaryGetTypeID() &&
                ::CFDictionaryGetValue(target, CFSTR("Statistics")) == nullptr) {
                ::CFDictionarySetValue(target, CFSTR("Statistics"), stats);
            }
            ::CFRelease(stats);
        }

        const bool all_found =
            ::CFDictionaryGetValue(target, CFSTR("Vendor")) != nullptr &&
            ::CFDictionaryGetValue(
                target, ::kDADiskDescriptionDeviceModelKey) != nullptr &&
            ::CFDictionaryGetValue(target, CFSTR("Revision")) != nullptr &&
            ::CFDictionaryGetValue(target, CFSTR("SMART Status")) != nullptr &&
            ::CFDictionaryGetValue(target, CFSTR("Statistics")) != nullptr;
        if (all_found) { return; }

        // Do not acquire a parent that no later iteration would own.
        if (level + 1 == maximum_levels) { return; }

        ::io_object_t parent = 0;
        if (::IORegistryEntryGetParentEntry(current, kIOServicePlane,
                                            &parent) != KERN_SUCCESS) {
            return;
        }
        current = parent;
    }
}

/// Builds one whole-media fact dictionary from the registry properties of
/// one media object plus the documented DiskArbitration description.
///
/// Returns null when the medium carries no recorded transport protocol or
/// records itself as image-backed virtual media; both cases stay outside
/// the enumeration contract. Every intermediate reference is released on
/// every path.
inline ::CFMutableDictionaryRef build_media_dictionary(
    ::io_service_t media, ::DASessionRef session) {
    ::CFBooleanRef whole = static_cast<::CFBooleanRef>(
        ::IORegistryEntryCreateCFProperty(media, CFSTR(kIOMediaWholeKey),
                                          ::kCFAllocatorDefault, 0));
    if (whole == nullptr) { return nullptr; }
    const bool is_whole = ::CFGetTypeID(whole) ==
                              ::CFBooleanGetTypeID() &&
                          whole == ::kCFBooleanTrue;
    ::CFRelease(whole);
    if (!is_whole) { return nullptr; }

    ::CFStringRef bsd_name = static_cast<::CFStringRef>(
        ::IORegistryEntryCreateCFProperty(media, CFSTR(kIOBSDNameKey),
                                          ::kCFAllocatorDefault, 0));
    if (bsd_name == nullptr ||
        ::CFGetTypeID(bsd_name) != ::CFStringGetTypeID() ||
        ::CFStringGetLength(bsd_name) == 0) {
        if (bsd_name != nullptr) { ::CFRelease(bsd_name); }
        return nullptr;
    }

    ::CFNumberRef size = static_cast<::CFNumberRef>(
        ::IORegistryEntryCreateCFProperty(media, CFSTR(kIOMediaSizeKey),
                                          ::kCFAllocatorDefault, 0));
    ::CFNumberRef block_size = static_cast<::CFNumberRef>(
        ::IORegistryEntryCreateCFProperty(
            media, CFSTR(kIOMediaPreferredBlockSizeKey),
            ::kCFAllocatorDefault, 0));
    ::CFBooleanRef ejectable = static_cast<::CFBooleanRef>(
        ::IORegistryEntryCreateCFProperty(
            media, CFSTR(kIOMediaEjectableKey), ::kCFAllocatorDefault, 0));

    ::CFMutableDictionaryRef entry = ::CFDictionaryCreateMutable(
        ::kCFAllocatorDefault, 0, &::kCFTypeDictionaryKeyCallBacks,
        &::kCFTypeDictionaryValueCallBacks);
    if (entry == nullptr) {
        if (ejectable != nullptr) { ::CFRelease(ejectable); }
        if (block_size != nullptr) { ::CFRelease(block_size); }
        if (size != nullptr) { ::CFRelease(size); }
        ::CFRelease(bsd_name);
        return nullptr;
    }

    ::CFDictionarySetValue(entry, kDADiskDescriptionMediaBSDNameKey,
                           bsd_name);
    if (size != nullptr) {
        ::CFDictionarySetValue(entry, kDADiskDescriptionMediaSizeKey,
                               size);
    }
    if (block_size != nullptr) {
        ::CFDictionarySetValue(entry,
                               kDADiskDescriptionMediaBlockSizeKey,
                               block_size);
    }
    if (ejectable != nullptr) {
        ::CFDictionarySetValue(entry,
                               kDADiskDescriptionMediaEjectableKey,
                               ejectable);
    }
    if (ejectable != nullptr) { ::CFRelease(ejectable); }
    if (block_size != nullptr) { ::CFRelease(block_size); }
    if (size != nullptr) { ::CFRelease(size); }
    ::CFRelease(bsd_name);

    const ::DADiskRef disk =
        ::DADiskCreateFromIOMedia(::kCFAllocatorDefault, session, media);
    bool excluded = false;
    if (disk != nullptr) {
        const ::CFDictionaryRef description =
            ::DADiskCopyDescription(disk);
        ::CFRelease(disk);
        if (description != nullptr) {
            const void* protocol = ::CFDictionaryGetValue(
                description, kDADiskDescriptionDeviceProtocolKey);
            const void* model = ::CFDictionaryGetValue(
                description, kDADiskDescriptionDeviceModelKey);
            if (protocol != nullptr &&
                ::CFGetTypeID(protocol) == ::CFStringGetTypeID()) {
                if (::CFStringCompare(
                        static_cast<::CFStringRef>(protocol),
                        // CFSTR accepts only literals, so the exclusion
                        // rendering is spelled out here.
                        CFSTR("Virtual"), 0) == ::kCFCompareEqualTo) {
                    excluded = true;
                } else {
                    ::CFDictionarySetValue(
                        entry, kDADiskDescriptionDeviceProtocolKey,
                        protocol);
                }
            }
            if (!excluded && model != nullptr &&
                ::CFGetTypeID(model) == ::CFStringGetTypeID() &&
                ::CFStringGetLength(static_cast<::CFStringRef>(model)) !=
                    0) {
                ::CFDictionarySetValue(
                    entry, kDADiskDescriptionDeviceModelKey, model);
            }
            ::CFRelease(description);
        }
    }
    if (!excluded && ::CFDictionaryGetValue(
                             entry, kDADiskDescriptionDeviceProtocolKey) ==
                         nullptr) {
        excluded = true;
    }
    if (!excluded) { gather_backing_identity(media, entry); }
    if (excluded) {
        ::CFRelease(entry);
        return nullptr;
    }
    return entry;
}

/// Collects partition records from partition media dictionaries.
template <typename DriveApi>
inline result<std::vector<storage_common::partition_record>>
collect_partitions() {
    const result<::CFArrayRef> collected = DriveApi::partition_media_facts();
    if (!collected) { return fail(collected.error()); }
    const cf_object owned(*collected);
    const ::CFArrayRef facts_array =
        static_cast<::CFArrayRef>(owned.get());

    std::vector<storage_common::partition_record> records;
    const ::CFIndex count = ::CFArrayGetCount(facts_array);
    for (::CFIndex index = 0; index < count; ++index) {
        const void* raw_dict = ::CFArrayGetValueAtIndex(facts_array, index);
        if (raw_dict == nullptr ||
            ::CFGetTypeID(raw_dict) != ::CFDictionaryGetTypeID()) {
            return fail(errc::malformed_data);
        }
        const ::CFDictionaryRef dictionary =
            static_cast<::CFDictionaryRef>(raw_dict);

        bool has_bsd_name = false;
        std::string bsd_name;
        const result<bool> copied_name = copy_optional_string(
            dictionary, ::kDADiskDescriptionMediaBSDNameKey, has_bsd_name,
            bsd_name);
        if (!copied_name) { return fail(copied_name.error()); }
        if (!has_bsd_name || bsd_name.empty()) {
            return fail(errc::malformed_data);
        }

        storage_common::partition_record record;
        record.identifier = bsd_name;

        bool has_parent_name = false;
        std::string parent_name;
        const result<bool> copied_parent = copy_optional_string(
            dictionary, CFSTR("SyscapeParentBSDName"), has_parent_name,
            parent_name);
        if (!copied_parent) { return fail(copied_parent.error()); }
        if (!has_parent_name || parent_name.empty()) {
            return fail(errc::malformed_data);
        }
        record.disk_identifier = parent_name;

        const std::string prefix = parent_name + "s";
        if (bsd_name.size() <= prefix.size() ||
            bsd_name.compare(0U, prefix.size(), prefix) != 0) {
            return fail(errc::malformed_data);
        }
        std::uint32_t number = 0U;
        for (std::size_t c = prefix.size(); c < bsd_name.size(); ++c) {
            if (bsd_name[c] < '0' || bsd_name[c] > '9') {
                return fail(errc::malformed_data);
            }
            const std::uint32_t digit =
                static_cast<std::uint32_t>(bsd_name[c] - '0');
            if (number >
                (std::numeric_limits<std::uint32_t>::max() - digit) / 10U) {
                return fail(errc::value_too_large);
            }
            number = number * 10U + digit;
        }
        if (number == 0U) { return fail(errc::malformed_data); }
        record.partition_number = number;

        std::uint64_t size_bytes = 0U;
        const result<bool> copied_size = copy_optional_number(
            dictionary, ::kDADiskDescriptionMediaSizeKey, size_bytes);
        if (!copied_size) { return fail(copied_size.error()); }
        if (*copied_size) {
            record.has_size_bytes = true;
            record.size_bytes = size_bytes;
        }

        std::uint64_t base_bytes = 0U;
        const result<bool> copied_base =
            copy_optional_number(dictionary, CFSTR("Base"), base_bytes);
        if (!copied_base) { return fail(copied_base.error()); }
        if (*copied_base) {
            record.has_start_offset_bytes = true;
            record.start_offset_bytes = base_bytes;
        }

        bool writable = true;
        const result<bool> copied_writable = copy_optional_boolean(
            dictionary, ::kDADiskDescriptionMediaWritableKey, writable);
        if (!copied_writable) { return fail(copied_writable.error()); }
        if (*copied_writable) {
            record.is_read_only = !writable;
        }

        bool has_content = false;
        std::string content;
        const result<bool> copied_content = copy_optional_string(
            dictionary, ::kDADiskDescriptionMediaContentKey, has_content,
            content);
        if (!copied_content) { return fail(copied_content.error()); }
        if (has_content) {
            record.has_type_identifier = true;
            record.type_identifier = content;
            // IOMedia content describes the slice contents, not the parent
            // disk's partition-table scheme. Preserve it as the platform's
            // type rendering without guessing GPT, MBR, or APM.
        }

        bool has_vol_name = false;
        std::string vol_name;
        const result<bool> copied_vol_name = copy_optional_string(
            dictionary, ::kDADiskDescriptionVolumeNameKey, has_vol_name,
            vol_name);
        if (!copied_vol_name) { return fail(copied_vol_name.error()); }
        if (has_vol_name) {
            record.has_name = true;
            record.name = std::move(vol_name);
        }

        bool has_vol_uuid = false;
        std::string vol_uuid;
        const result<bool> copied_vol_uuid = copy_optional_uuid(
            dictionary, ::kDADiskDescriptionVolumeUUIDKey, has_vol_uuid,
            vol_uuid);
        if (!copied_vol_uuid) { return fail(copied_vol_uuid.error()); }
        if (has_vol_uuid) {
            record.has_uuid = true;
            record.uuid = std::move(vol_uuid);
        }

        bool has_vol_path = false;
        std::string vol_path;
        const result<bool> copied_vol_path = copy_optional_url_path(
            dictionary, ::kDADiskDescriptionVolumePathKey, has_vol_path,
            vol_path);
        if (!copied_vol_path) { return fail(copied_vol_path.error()); }
        if (has_vol_path && !vol_path.empty()) {
            record.is_mounted = true;
            record.mount_point = std::move(vol_path);
        }

        records.push_back(std::move(record));
    }

    std::sort(records.begin(), records.end(),
              [](const storage_common::partition_record& left,
                 const storage_common::partition_record& right) {
                  return left.identifier < right.identifier;
              });
    return records;
}

/// Builds one partition-media fact dictionary from IOKit and DiskArbitration.
inline ::CFMutableDictionaryRef build_partition_dictionary(
    ::io_service_t media, ::DASessionRef session) {
    ::CFBooleanRef whole = static_cast<::CFBooleanRef>(
        ::IORegistryEntryCreateCFProperty(media, CFSTR(kIOMediaWholeKey),
                                          ::kCFAllocatorDefault, 0));
    if (whole == nullptr) { return nullptr; }
    const bool has_boolean_whole =
        ::CFGetTypeID(whole) == ::CFBooleanGetTypeID();
    const bool is_whole = has_boolean_whole && whole == ::kCFBooleanTrue;
    ::CFRelease(whole);
    if (!has_boolean_whole || is_whole) { return nullptr; }

    ::CFStringRef bsd_name = static_cast<::CFStringRef>(
        ::IORegistryEntryCreateCFProperty(media, CFSTR(kIOBSDNameKey),
                                          ::kCFAllocatorDefault, 0));
    if (bsd_name == nullptr ||
        ::CFGetTypeID(bsd_name) != ::CFStringGetTypeID() ||
        ::CFStringGetLength(bsd_name) == 0) {
        if (bsd_name != nullptr) { ::CFRelease(bsd_name); }
        return nullptr;
    }

    ::CFNumberRef size = static_cast<::CFNumberRef>(
        ::IORegistryEntryCreateCFProperty(media, CFSTR(kIOMediaSizeKey),
                                          ::kCFAllocatorDefault, 0));
    ::CFNumberRef base =
        static_cast<::CFNumberRef>(::IORegistryEntryCreateCFProperty(
            media, CFSTR("Base"), ::kCFAllocatorDefault, 0));
    ::CFStringRef content = static_cast<::CFStringRef>(
        ::IORegistryEntryCreateCFProperty(media, CFSTR(kIOMediaContentKey),
                                          ::kCFAllocatorDefault, 0));

    ::CFMutableDictionaryRef entry = ::CFDictionaryCreateMutable(
        ::kCFAllocatorDefault, 0, &::kCFTypeDictionaryKeyCallBacks,
        &::kCFTypeDictionaryValueCallBacks);
    if (entry == nullptr) {
        if (content != nullptr) { ::CFRelease(content); }
        if (base != nullptr) { ::CFRelease(base); }
        if (size != nullptr) { ::CFRelease(size); }
        ::CFRelease(bsd_name);
        return nullptr;
    }

    ::CFDictionarySetValue(entry, kDADiskDescriptionMediaBSDNameKey, bsd_name);
    if (size != nullptr) {
        ::CFDictionarySetValue(entry, kDADiskDescriptionMediaSizeKey, size);
        ::CFRelease(size);
    }
    if (base != nullptr) {
        ::CFDictionarySetValue(entry, CFSTR("Base"), base);
        ::CFRelease(base);
    }
    if (content != nullptr) {
        ::CFDictionarySetValue(entry, kDADiskDescriptionMediaContentKey,
                               content);
        ::CFRelease(content);
    }
    ::CFRelease(bsd_name);

    const ::DADiskRef disk =
        ::DADiskCreateFromIOMedia(::kCFAllocatorDefault, session, media);
    if (disk == nullptr) {
        ::CFRelease(entry);
        return nullptr;
    }
    const cf_object owned_disk(disk);

    // Resolve the actual whole disk and require the same non-virtual,
    // recorded protocol that qualifies an entry for drives(). This excludes
    // image-backed media and synthesized APFS volume trees from the physical
    // partition contract.
    const ::DADiskRef whole_disk = ::DADiskCopyWholeDisk(disk);
    if (whole_disk == nullptr) {
        ::CFRelease(entry);
        return nullptr;
    }
    const cf_object owned_whole_disk(whole_disk);
    const char* parent_bsd_name = ::DADiskGetBSDName(whole_disk);
    const ::CFDictionaryRef whole_description =
        ::DADiskCopyDescription(whole_disk);
    if (parent_bsd_name == nullptr || whole_description == nullptr) {
        if (whole_description != nullptr) { ::CFRelease(whole_description); }
        ::CFRelease(entry);
        return nullptr;
    }
    const cf_object owned_whole_description(whole_description);
    const void* protocol = ::CFDictionaryGetValue(
        whole_description, kDADiskDescriptionDeviceProtocolKey);
    if (protocol == nullptr ||
        ::CFGetTypeID(protocol) != ::CFStringGetTypeID() ||
        ::CFStringGetLength(static_cast<::CFStringRef>(protocol)) == 0 ||
        ::CFStringCompare(static_cast<::CFStringRef>(protocol),
                          CFSTR("Virtual"), 0) == ::kCFCompareEqualTo) {
        ::CFRelease(entry);
        return nullptr;
    }
    const ::CFStringRef parent_name = ::CFStringCreateWithCString(
        ::kCFAllocatorDefault, parent_bsd_name, ::kCFStringEncodingUTF8);
    if (parent_name == nullptr) {
        ::CFRelease(entry);
        return nullptr;
    }
    ::CFDictionarySetValue(entry, CFSTR("SyscapeParentBSDName"), parent_name);
    ::CFRelease(parent_name);

    const ::CFDictionaryRef description = ::DADiskCopyDescription(disk);
    if (description != nullptr) {
        const void* vol_name = ::CFDictionaryGetValue(
            description, kDADiskDescriptionVolumeNameKey);
        if (vol_name != nullptr) {
            ::CFDictionarySetValue(entry,
                                   kDADiskDescriptionVolumeNameKey,
                                   vol_name);
        }
        const void* vol_uuid = ::CFDictionaryGetValue(
            description, kDADiskDescriptionVolumeUUIDKey);
        if (vol_uuid != nullptr) {
            ::CFDictionarySetValue(entry,
                                   kDADiskDescriptionVolumeUUIDKey,
                                   vol_uuid);
        }
        const void* vol_path = ::CFDictionaryGetValue(
            description, kDADiskDescriptionVolumePathKey);
        if (vol_path != nullptr) {
            ::CFDictionarySetValue(entry,
                                   kDADiskDescriptionVolumePathKey,
                                   vol_path);
        }
        const void* writable = ::CFDictionaryGetValue(
            description, kDADiskDescriptionMediaWritableKey);
        if (writable != nullptr) {
            ::CFDictionarySetValue(entry,
                                   kDADiskDescriptionMediaWritableKey,
                                   writable);
        }
        ::CFRelease(description);
    }

    return entry;
}

inline result<::CFArrayRef> native_drive_api::whole_media_facts() {
    ::io_iterator_t raw_iterator = 0;
    const ::kern_return_t matched = ::IOServiceGetMatchingServices(
        MACH_PORT_NULL, ::IOServiceMatching("IOMedia"), &raw_iterator);
    if (matched != KERN_SUCCESS) {
        return fail(errc::io_error);
    }
    const io_object iterator(raw_iterator);

    const ::DASessionRef raw_session = ::DASessionCreate(::kCFAllocatorDefault);
    if (raw_session == nullptr) {
        return fail(errc::io_error);
    }
    const cf_object session(raw_session);

    ::CFMutableArrayRef facts = ::CFArrayCreateMutable(
        ::kCFAllocatorDefault, 0, &::kCFTypeArrayCallBacks);
    if (facts == nullptr) {
        return fail(errc::io_error);
    }

    for (;;) {
        const ::io_object_t media = ::IOIteratorNext(iterator.get());
        if (media == 0) {
            break;
        }
        const io_object owned_media(media);
        ::CFMutableDictionaryRef entry = build_media_dictionary(
            static_cast<::io_service_t>(media), raw_session);
        if (entry == nullptr) {
            continue;
        }
        ::CFArrayAppendValue(facts, entry);
        ::CFRelease(entry);
    }
    return facts;
}

inline result<::CFArrayRef> native_drive_api::partition_media_facts() {
    ::io_iterator_t raw_iterator = 0;
    const ::kern_return_t matched = ::IOServiceGetMatchingServices(
        MACH_PORT_NULL, ::IOServiceMatching("IOMedia"), &raw_iterator);
    if (matched != KERN_SUCCESS) {
        return fail(errc::io_error);
    }
    const io_object iterator(raw_iterator);

    const ::DASessionRef raw_session = ::DASessionCreate(::kCFAllocatorDefault);
    if (raw_session == nullptr) {
        return fail(errc::io_error);
    }
    const cf_object session(raw_session);

    ::CFMutableArrayRef facts = ::CFArrayCreateMutable(
        ::kCFAllocatorDefault, 0, &::kCFTypeArrayCallBacks);
    if (facts == nullptr) { return fail(errc::io_error); }

    for (;;) {
        const ::io_object_t media = ::IOIteratorNext(iterator.get());
        if (media == 0) { break; }
        const io_object owned_media(media);
        ::CFMutableDictionaryRef entry = build_partition_dictionary(
            static_cast<::io_service_t>(media), raw_session);
        if (entry == nullptr) { continue; }
        ::CFArrayAppendValue(facts, entry);
        ::CFRelease(entry);
    }
    return facts;
}

/// Returns one record per hardware-backed whole-disk drive recorded by
/// the platform.
inline result<std::vector<storage_common::drive_record>> drives() {
    return collect_drives<native_drive_api>();
}

/// Returns one record per partition recorded by the platform.
inline result<std::vector<storage_common::partition_record>> partitions() {
    return collect_partitions<native_drive_api>();
}

/// Parses a SMART Status string from IOKit.
inline bool parse_macos_smart_status(
    std::string_view status_str, storage_common::health_record& record) noexcept {
    if (status_str == "Verified") {
        record.has_failure_predicted = true;
        record.failure_predicted = false;
        record.status = storage_common::health_status_classification::healthy;
        return true;
    }
    if (status_str == "Failing" || status_str == "Failure" ||
        status_str == "Fatal") {
        record.has_failure_predicted = true;
        record.failure_predicted = true;
        record.status = storage_common::health_status_classification::warning;
        return true;
    }
    record.has_failure_predicted = false;
    record.status = storage_common::health_status_classification::unknown;
    return false;
}

/// Parses IOKit Statistics dictionary for bytes read and written.
inline void parse_macos_statistics(
    ::CFDictionaryRef stats, storage_common::health_record& record) {
    if (stats == nullptr) { return; }
    const void* read_bytes =
        ::CFDictionaryGetValue(stats, CFSTR("Bytes (Read)"));
    if (read_bytes != nullptr &&
        ::CFGetTypeID(read_bytes) == ::CFNumberGetTypeID()) {
        std::int64_t val = 0;
        if (::CFNumberGetValue(static_cast<::CFNumberRef>(read_bytes),
                               ::kCFNumberSInt64Type, &val) && val >= 0) {
            record.has_data_units_read_bytes = true;
            record.data_units_read_bytes = static_cast<std::uint64_t>(val);
        }
    }
    const void* write_bytes =
        ::CFDictionaryGetValue(stats, CFSTR("Bytes (Written)"));
    if (write_bytes != nullptr &&
        ::CFGetTypeID(write_bytes) == ::CFNumberGetTypeID()) {
        std::int64_t val = 0;
        if (::CFNumberGetValue(static_cast<::CFNumberRef>(write_bytes),
                               ::kCFNumberSInt64Type, &val) && val >= 0) {
            record.has_data_units_written_bytes = true;
            record.data_units_written_bytes = static_cast<std::uint64_t>(val);
        }
    }
}

/// Converts one whole-media dictionary into a health record.
inline result<bool> convert_health_entry(
    ::CFDictionaryRef dictionary, storage_common::health_record& record) {
    bool has_bsd_name = false;
    std::string bsd_name;
    const result<bool> copied_name = copy_optional_string(
        dictionary, kDADiskDescriptionMediaBSDNameKey, has_bsd_name, bsd_name);
    if (!copied_name) { return fail(copied_name.error()); }
    if (!has_bsd_name || bsd_name.empty()) { return fail(errc::malformed_data); }

    record.identifier = std::move(bsd_name);
    record.status = storage_common::health_status_classification::unknown;
    record.has_failure_predicted = false;

    bool has_smart = false;
    std::string smart_str;
    const result<bool> copied_smart = copy_optional_string(
        dictionary, CFSTR("SMART Status"), has_smart, smart_str);
    if (!copied_smart) { return fail(copied_smart.error()); }
    if (has_smart) {
        parse_macos_smart_status(smart_str, record);
    }

    const void* stats = ::CFDictionaryGetValue(dictionary, CFSTR("Statistics"));
    if (stats != nullptr && ::CFGetTypeID(stats) == ::CFDictionaryGetTypeID()) {
        parse_macos_statistics(static_cast<::CFDictionaryRef>(stats), record);
    }

    return true;
}

/// Enumerates health records for all drives on macOS.
template <typename DriveApi>
inline result<std::vector<storage_common::health_record>>
collect_all_drive_health() {
    const result<::CFArrayRef> collected = DriveApi::whole_media_facts();
    if (!collected) { return fail(collected.error()); }
    const cf_object owned(*collected);
    const ::CFArrayRef facts_array =
        static_cast<::CFArrayRef>(owned.get());

    std::vector<storage_common::health_record> records;
    const ::CFIndex count = ::CFArrayGetCount(facts_array);
    for (::CFIndex index = 0; index < count; ++index) {
        const void* raw_dict = ::CFArrayGetValueAtIndex(facts_array, index);
        if (raw_dict == nullptr ||
            ::CFGetTypeID(raw_dict) != ::CFDictionaryGetTypeID()) {
            return fail(errc::malformed_data);
        }
        const ::CFDictionaryRef dictionary =
            static_cast<::CFDictionaryRef>(raw_dict);

        storage_common::health_record record;
        const result<bool> converted = convert_health_entry(dictionary, record);
        if (!converted) { return fail(converted.error()); }
        records.push_back(std::move(record));
    }

    std::sort(records.begin(), records.end(),
              [](const storage_common::health_record& left,
                 const storage_common::health_record& right) {
                  return left.identifier < right.identifier;
              });
    return records;
}

/// Queries health facts for a single drive on macOS.
template <typename DriveApi>
inline result<storage_common::health_record> collect_drive_health(
    std::string_view disk_identifier) {
    const result<std::vector<storage_common::health_record>> all =
        collect_all_drive_health<DriveApi>();
    if (!all) { return fail(all.error()); }
    for (const auto& r : *all) {
        if (r.identifier == disk_identifier) {
            return r;
        }
    }
    return fail(errc::not_found);
}

inline result<storage_common::health_record> health(
    std::string_view disk_identifier) {
    if (!storage_common::is_valid_disk_identifier(disk_identifier)) {
        return fail(errc::invalid_argument);
    }
    return collect_drive_health<native_drive_api>(disk_identifier);
}

inline result<std::vector<storage_common::health_record>> all_drive_health() {
    return collect_all_drive_health<native_drive_api>();
}

} // namespace storage_backend
} // namespace detail
} // namespace syscape

#endif
