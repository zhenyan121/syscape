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
    if (::IORegistryEntryGetParentEntry(media, ::kIOServicePlane,
                                        &current) != ::KERN_SUCCESS) {
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

        const bool all_found =
            ::CFDictionaryGetValue(target, CFSTR("Vendor")) != nullptr &&
            ::CFDictionaryGetValue(
                target, ::kDADiskDescriptionDeviceModelKey) != nullptr &&
            ::CFDictionaryGetValue(target, CFSTR("Revision")) != nullptr;
        if (all_found) { return; }

        // Do not acquire a parent that no later iteration would own.
        if (level + 1 == maximum_levels) { return; }

        ::io_object_t parent = 0;
        if (::IORegistryEntryGetParentEntry(current, ::kIOServicePlane,
                                            &parent) != ::KERN_SUCCESS) {
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

/// Returns an owned array of whole-media fact dictionaries built from the
/// IOKit registry and the documented DiskArbitration description
/// interface.
result<::CFArrayRef> native_drive_api::whole_media_facts() {
    ::io_iterator_t raw_iterator = 0;
    const ::kern_return_t matched = ::IOServiceGetMatchingServices(
        ::kIOMasterPortDefault, ::IOServiceMatching("IOMedia"),
        &raw_iterator);
    if (matched != ::KERN_SUCCESS) { return fail(errc::io_error); }
    const io_object iterator(raw_iterator);

    const cf_object session(::DASessionCreate(::kCFAllocatorDefault));
    if (session.get() == nullptr) { return fail(errc::io_error); }

    ::CFMutableArrayRef facts = ::CFArrayCreateMutable(
        ::kCFAllocatorDefault, 0, &::kCFTypeArrayCallBacks);
    if (facts == nullptr) { return fail(errc::io_error); }

    for (;;) {
        const ::io_object_t media = ::IOIteratorNext(iterator.get());
        if (media == 0) { break; }
        const io_object owned_media(media);
        ::CFMutableDictionaryRef entry = build_media_dictionary(
            static_cast<::io_service_t>(media),
            static_cast<::DASessionRef>(session.get()));
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

} // namespace storage_backend
} // namespace detail
} // namespace syscape

#endif
