#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <syscape/storage.hpp>
#include <syscape/detail/storage/macos.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

/// Creates an owned CoreFoundation string from UTF-8 text.
::CFStringRef make_string(const char* text) {
    return ::CFStringCreateWithCString(::kCFAllocatorDefault, text,
                                       ::kCFStringEncodingUTF8);
}

/// Creates an owned CoreFoundation number from a signed 64-bit value.
::CFNumberRef make_number(long long value) {
    return ::CFNumberCreate(::kCFAllocatorDefault, ::kCFNumberLongLongType,
                            &value);
}

/// Builds one media-facts dictionary; null arguments record absent keys.
::CFDictionaryRef make_media_dictionary(const char* bsd_name,
                                        const char* protocol,
                                        const long long* size,
                                        const long long* block_size,
                                        bool ejectable_present,
                                        bool ejectable,
                                        const char* model,
                                        const char* vendor,
                                        const char* revision) {
    ::CFMutableDictionaryRef dictionary = ::CFDictionaryCreateMutable(
        ::kCFAllocatorDefault, 0, &::kCFTypeDictionaryKeyCallBacks,
        &::kCFTypeDictionaryValueCallBacks);

    const auto set_string = [dictionary](const void* key, const char* text) {
        if (text == nullptr) { return; }
        const ::CFStringRef value = make_string(text);
        ::CFDictionarySetValue(dictionary, key, value);
        ::CFRelease(value);
    };
    const auto set_number = [dictionary](const void* key,
                                         const long long* value) {
        if (value == nullptr) { return; }
        const ::CFNumberRef number = make_number(*value);
        ::CFDictionarySetValue(dictionary, key, number);
        ::CFRelease(number);
    };

    set_string(::kDADiskDescriptionMediaBSDNameKey, bsd_name);
    set_string(::kDADiskDescriptionDeviceProtocolKey, protocol);
    set_number(::kDADiskDescriptionMediaSizeKey, size);
    set_number(::kDADiskDescriptionMediaBlockSizeKey, block_size);
    if (ejectable_present) {
        ::CFDictionarySetValue(
            dictionary, ::kDADiskDescriptionMediaEjectableKey,
            ejectable ? ::kCFBooleanTrue : ::kCFBooleanFalse);
    }
    set_string(::kDADiskDescriptionDeviceModelKey, model);
    set_string(::CFSTR("Vendor"), vendor);
    set_string(::CFSTR("Revision"), revision);
    return dictionary;
}

void test_protocol_classification() {
    namespace backend = syscape::detail::storage_backend;
    using syscape::storage::bus_type;

    expect(backend::classify_protocol("ATA") == bus_type::sata,
           "The documented ATA protocol rendering must map onto sata");
    expect(backend::classify_protocol("ATAPI") == bus_type::atapi,
           "The documented ATAPI rendering must map onto atapi");
    expect(backend::classify_protocol("USB") == bus_type::usb,
           "The documented USB rendering must map onto usb");
    expect(backend::classify_protocol("FireWire") == bus_type::firewire,
           "The documented FireWire rendering must map onto firewire");
    expect(backend::classify_protocol("Secure Digital") == bus_type::sd,
           "The documented Secure Digital rendering must map onto sd");
    expect(backend::classify_protocol("NVMe") == bus_type::nvme,
           "The documented NVMe rendering must map onto nvme");
    expect(backend::classify_protocol("RAID") == bus_type::raid,
           "The documented RAID rendering must map onto raid");
    expect(backend::classify_protocol("Something new") ==
               bus_type::unknown,
           "Unfamiliar protocol renderings must stay unknown rather than "
           "being guessed");
}

void test_media_conversion() {
    namespace backend = syscape::detail::storage_backend;

    long long size = 500107862016LL;
    long long block_size = 4096;
    const ::CFDictionaryRef full = make_media_dictionary(
        "disk0", "USB", &size, &block_size, true, false, "Model X",
        "Vendor Y", "rev9");
    const backend::media_facts facts = *backend::convert_media_dictionary(
        full);
    ::CFRelease(full);

    expect(facts.identifier == "disk0",
           "The BSD-name rendering must become the identifier");
    expect(facts.protocol == "USB",
           "The recorded protocol must pass through verbatim");
    expect(!facts.virtual_media,
           "Hardware protocols must not mark media as virtual");
    expect(facts.capacity_bytes == 500107862016ULL,
           "The recorded size must surface as the byte capacity");
    expect(facts.has_block_size && facts.block_size_bytes == 4096U,
           "A recorded preferred block size must pass through");
    expect(!facts.ejectable,
           "An explicit non-ejectable recording must stay false");
    expect(facts.has_model && facts.model == "Model X",
           "The device-model rendering must pass through");
    expect(facts.has_vendor && facts.vendor == "Vendor Y",
           "The registry vendor rendering must pass through");
    expect(facts.has_revision && facts.revision == "rev9",
           "The registry revision rendering must pass through");

    const ::CFDictionaryRef minimal =
        make_media_dictionary("disk1", "NVMe", &size, nullptr, false,
                              false, nullptr, nullptr, nullptr);
    const backend::media_facts sparse = *backend::convert_media_dictionary(
        minimal);
    ::CFRelease(minimal);
    expect(sparse.has_model == false && sparse.has_vendor == false &&
               sparse.has_revision == false,
           "Absent identity keys must leave every text field unrecorded");
    expect(sparse.has_block_size == false,
           "An absent preferred-block-size key must leave the size "
           "unrecorded");
    expect(sparse.ejectable == false,
           "An absent ejectability key carries the platform's explicit "
           "non-ejectable default");

    const ::CFDictionaryRef image =
        make_media_dictionary("disk2", "Virtual", &size, nullptr, true,
                              true, nullptr, nullptr, nullptr);
    const backend::media_facts virtual_facts =
        *backend::convert_media_dictionary(image);
    ::CFRelease(image);
    expect(virtual_facts.virtual_media,
           "The Virtual protocol must mark image-backed media so the "
           "collector can exclude it");

    const ::CFDictionaryRef anonymous =
        make_media_dictionary(nullptr, "USB", &size, nullptr, false,
                              false, nullptr, nullptr, nullptr);
    const auto unnamed = backend::convert_media_dictionary(anonymous);
    ::CFRelease(anonymous);
    expect(!unnamed &&
               unnamed.error() == syscape::errc::malformed_data,
           "Media without a BSD name cannot satisfy the enumeration "
           "contract and must be malformed data");

    const ::CFDictionaryRef untransported =
        make_media_dictionary("disk3", nullptr, &size, nullptr, false,
                              false, nullptr, nullptr, nullptr);
    const auto transportless =
        backend::convert_media_dictionary(untransported);
    ::CFRelease(untransported);
    expect(!transportless &&
               transportless.error() == syscape::errc::malformed_data,
           "Media without a recorded protocol cannot be classified and "
           "must be malformed data");

    const ::CFDictionaryRef unsized =
        make_media_dictionary("disk4", "USB", nullptr, nullptr, false,
                              false, nullptr, nullptr, nullptr);
    const auto measureless =
        backend::convert_media_dictionary(unsized);
    ::CFRelease(unsized);
    expect(!measureless &&
               measureless.error() == syscape::errc::malformed_data,
           "Whole-disk media always carry their size; its absence is "
           "malformed platform data");

    long long negative = -1LL;
    const ::CFDictionaryRef shrunken = make_media_dictionary(
        "disk5", "USB", &negative, nullptr, false, false, nullptr,
        nullptr, nullptr);
    const auto impossible =
        backend::convert_media_dictionary(shrunken);
    ::CFRelease(shrunken);
    expect(!impossible &&
               impossible.error() == syscape::errc::malformed_data,
           "A negative size contradicts the notion of capacity and must "
           "be malformed platform data");

    const ::CFMutableDictionaryRef wrong_typed =
        ::CFDictionaryCreateMutable(
            ::kCFAllocatorDefault, 0, &::kCFTypeDictionaryKeyCallBacks,
            &::kCFTypeDictionaryValueCallBacks);
    {
        const ::CFStringRef name = make_string("disk6");
        const ::CFNumberRef not_a_name = make_number(6);
        ::CFDictionarySetValue(
            wrong_typed, ::kDADiskDescriptionMediaBSDNameKey,
            not_a_name);
        ::CFRelease(not_a_name);
        ::CFRelease(name);
    }
    const auto misrendered =
        backend::convert_media_dictionary(wrong_typed);
    ::CFRelease(wrong_typed);
    expect(!misrendered &&
               misrendered.error() == syscape::errc::malformed_data,
           "A wrong-typed field contradicts the platform's own schema");
}

/// Builds an owned CFArray that retains every fabricated dictionary.
::CFArrayRef retain_all(const std::vector<::CFDictionaryRef>& source) {
    ::CFMutableArrayRef array = ::CFArrayCreateMutable(
        ::kCFAllocatorDefault, 0, &::kCFTypeArrayCallBacks);
    for (::CFDictionaryRef entry : source) {
        ::CFArrayAppendValue(array, entry);
    }
    return array;
}

/// Replays a mixed population: two hardware drives plus one image-backed
/// virtual disk enumerated out of order.
struct mixed_api {
    /// The fabricated population, filled by the test before collection.
    static std::vector<::CFDictionaryRef>& entries() {
        static std::vector<::CFDictionaryRef> populated;
        return populated;
    }

    static syscape::result<::CFArrayRef> whole_media_facts() {
        return retain_all(entries());
    }
};

/// Replays a machine without any drive.
struct empty_machine_api {
    static syscape::result<::CFArrayRef> whole_media_facts() {
        return retain_all(std::vector<::CFDictionaryRef>{});
    }
};

void test_collection() {
    namespace backend = syscape::detail::storage_backend;

    long long large_size = 1000204886016LL;
    long long small_size = 30055924736LL;
    long long huge_block = 1LL << 40;

    const ::CFDictionaryRef second = make_media_dictionary(
        "disk1", "ATA", &small_size, nullptr, true, true, "Spinning",
        nullptr, nullptr);
    const ::CFDictionaryRef first = make_media_dictionary(
        "disk0", "NVMe", &large_size, &huge_block, false, false,
        "Solid", "Chips", "a1");
    const ::CFDictionaryRef image = make_media_dictionary(
        "disk9", "Virtual", &small_size, nullptr, true, true, nullptr,
        nullptr, nullptr);

    mixed_api::entries().push_back(second);
    mixed_api::entries().push_back(first);
    mixed_api::entries().push_back(image);

    const syscape::result<std::vector<backend::storage_common::drive_record>>
        listed = backend::collect_drives<mixed_api>();
    expect(listed.has_value(),
           "A synthetic machine must enumerate successfully");
    if (!listed) { return; }

    expect(listed->size() == 2U,
           "Image-backed virtual media must stay excluded from the drive "
           "population");
    if (listed->size() != 2U) { return; }

    expect((*listed)[0].identifier == "disk0" &&
               (*listed)[1].identifier == "disk1",
           "Entries must be ordered by ascending identifier regardless of "
           "enumeration order");

    expect((*listed)[0].bus == syscape::storage::bus_type::nvme,
           "Fabricated protocol renderings must map through the "
           "classifier");
    expect((*listed)[0].has_capacity_bytes &&
               (*listed)[0].capacity_bytes == 1000204886016ULL,
           "Recorded sizes must surface as byte capacities");
    expect(!(*listed)[0].has_logical_sector_size_bytes,
           "Block sizes beyond the 32-bit field must stay unrecorded "
           "instead of truncating");
    expect((*listed)[0].removable == false,
           "Fixed media must report non-removable");
    expect((*listed)[0].has_model && (*listed)[0].model == "Solid" &&
               (*listed)[0].has_vendor &&
               (*listed)[0].vendor == "Chips" &&
               (*listed)[0].has_firmware_revision &&
               (*listed)[0].firmware_revision == "a1",
           "Identity renderings must pass through to the record");
    expect((*listed)[1].bus == syscape::storage::bus_type::sata,
           "The ATA rendering maps onto sata");
    expect((*listed)[1].removable == true,
           "Ejectable media must report removable");

    const syscape::result<std::vector<backend::storage_common::drive_record>>
        nothing = backend::collect_drives<empty_machine_api>();
    expect(nothing.has_value() && nothing->empty(),
           "An empty enumeration is valid data and must be accepted");

    ::CFRelease(second);
    ::CFRelease(first);
    ::CFRelease(image);
}

::CFDictionaryRef make_partition_dictionary(const char* bsd_name,
                                            const long long* size,
                                            const long long* base,
                                            const char* content,
                                            bool writable,
                                            const char* vol_name,
                                            const char* vol_uuid,
                                            const char* vol_path) {
    ::CFMutableDictionaryRef dictionary = ::CFDictionaryCreateMutable(
        ::kCFAllocatorDefault, 0, &::kCFTypeDictionaryKeyCallBacks,
        &::kCFTypeDictionaryValueCallBacks);

    const auto set_string = [dictionary](const void* key, const char* text) {
        if (text == nullptr) { return; }
        const ::CFStringRef value = make_string(text);
        ::CFDictionarySetValue(dictionary, key, value);
        ::CFRelease(value);
    };
    const auto set_number = [dictionary](const void* key,
                                         const long long* value) {
        if (value == nullptr) { return; }
        const ::CFNumberRef number = make_number(*value);
        ::CFDictionarySetValue(dictionary, key, number);
        ::CFRelease(number);
    };

    set_string(::kDADiskDescriptionMediaBSDNameKey, bsd_name);
    if (bsd_name != nullptr) {
        const char* separator = std::strrchr(bsd_name, 's');
        if (separator != nullptr && separator != bsd_name) {
            const std::string parent(
                bsd_name, static_cast<std::size_t>(separator - bsd_name));
            set_string(CFSTR("SyscapeParentBSDName"), parent.c_str());
        }
    }
    set_number(::kDADiskDescriptionMediaSizeKey, size);
    set_number(CFSTR(kIOMediaBaseKey), base);
    set_string(::kDADiskDescriptionMediaContentKey, content);
    ::CFDictionarySetValue(
        dictionary, ::kDADiskDescriptionMediaWritableKey,
        writable ? ::kCFBooleanTrue : ::kCFBooleanFalse);
    set_string(::kDADiskDescriptionVolumeNameKey, vol_name);

    if (vol_uuid != nullptr) {
        const ::CFStringRef uuid_str = make_string(vol_uuid);
        const ::CFUUIDRef uuid_ref = ::CFUUIDCreateFromString(
            ::kCFAllocatorDefault, uuid_str);
        ::CFRelease(uuid_str);
        if (uuid_ref != nullptr) {
            ::CFDictionarySetValue(dictionary,
                                   ::kDADiskDescriptionVolumeUUIDKey,
                                   uuid_ref);
            ::CFRelease(uuid_ref);
        }
    }

    if (vol_path != nullptr) {
        const ::CFURLRef url_ref = ::CFURLCreateFromFileSystemRepresentation(
            ::kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(vol_path),
            static_cast<CFIndex>(std::strlen(vol_path)), true);
        if (url_ref != nullptr) {
            ::CFDictionarySetValue(dictionary,
                                   ::kDADiskDescriptionVolumePathKey,
                                   url_ref);
            ::CFRelease(url_ref);
        }
    }

    return dictionary;
}

struct synthetic_partition_api {
    static syscape::result<::CFArrayRef> partition_media_facts() {
        static std::vector<::CFDictionaryRef> entries;
        if (entries.empty()) {
            long long p1_size = 536870912LL;
            long long p1_base = 2097152LL;
            long long p2_size = 1000000000000LL;
            long long p2_base = 538968064LL;

            entries.push_back(make_partition_dictionary(
                "disk0s2", &p2_size, &p2_base, "Apple_APFS", true,
                "Macintosh HD", "E61B23F3-E716-41B0-9A1D-3C9A57FA28C1",
                "/System/Volumes/Data"));
            entries.push_back(make_partition_dictionary(
                "disk0s1", &p1_size, &p1_base, "EFI", true, "EFI",
                "01234567-89AB-CDEF-0123-456789ABCDEF", nullptr));
        }
        return retain_all(entries);
    }
};

void test_partition_collection() {
    namespace backend = syscape::detail::storage_backend;

    const auto partitions_res =
        backend::collect_partitions<synthetic_partition_api>();
    expect(partitions_res.has_value(),
           "Synthetic partition collection must succeed");
    if (!partitions_res) { return; }

    expect(partitions_res->size() == 2U,
           "Both synthetic partitions must be returned");
    if (partitions_res->size() == 2U) {
        const auto& p0 = (*partitions_res)[0];
        expect(p0.identifier == "disk0s1",
               "Partitions must be sorted by identifier: disk0s1 first");
        expect(p0.disk_identifier == "disk0",
               "disk0s1 must have parent disk0");
        expect(p0.partition_number == 1U,
               "disk0s1 must have partition number 1");
        expect(p0.scheme == syscape::storage::partition_scheme::unknown,
               "Partition contents alone must not guess a table scheme");
        expect(p0.has_start_offset_bytes &&
                   p0.start_offset_bytes == 2097152ULL,
               "disk0s1 start offset must match");

        const auto& p1 = (*partitions_res)[1];
        expect(p1.identifier == "disk0s2",
               "disk0s2 must be second in order");
        expect(p1.partition_number == 2U,
               "disk0s2 must have partition number 2");
        expect(p1.is_mounted, "disk0s2 must report mounted");
        expect(p1.mount_point == "/System/Volumes/Data",
               "disk0s2 mount point must match");
        expect(p1.has_name && p1.name == "Macintosh HD",
               "disk0s2 volume name must match");
    }
}

void test_smart_status_parsing() {
    namespace backend = syscape::detail::storage_backend;
    namespace common = syscape::detail::storage_common;

    common::health_record verified;
    const bool p1 = backend::parse_macos_smart_status("Verified", verified);
    expect(p1 && verified.has_failure_predicted && !verified.failure_predicted &&
               verified.status == common::health_status_classification::healthy,
           "Verified status must convert to healthy");

    common::health_record failing;
    const bool p2 = backend::parse_macos_smart_status("Failing", failing);
    expect(p2 && failing.has_failure_predicted && failing.failure_predicted &&
               failing.status == common::health_status_classification::warning,
           "Failing status must convert to warning");

    common::health_record unknown;
    const bool p3 = backend::parse_macos_smart_status("Other", unknown);
    expect(!p3 && unknown.status == common::health_status_classification::unknown,
           "Unrecognized status must map to unknown");
}

void test_macos_statistics_parsing() {
    namespace backend = syscape::detail::storage_backend;
    namespace common = syscape::detail::storage_common;

    ::CFMutableDictionaryRef stats = ::CFDictionaryCreateMutable(
        ::kCFAllocatorDefault, 0, &::kCFTypeDictionaryKeyCallBacks,
        &::kCFTypeDictionaryValueCallBacks);
    long long read_val = 10485760LL;
    long long write_val = 5242880LL;
    ::CFNumberRef n_read = ::CFNumberCreate(
        ::kCFAllocatorDefault, ::kCFNumberLongLongType, &read_val);
    ::CFNumberRef n_write = ::CFNumberCreate(
        ::kCFAllocatorDefault, ::kCFNumberLongLongType, &write_val);
    ::CFDictionarySetValue(stats, CFSTR("Bytes (Read)"), n_read);
    ::CFDictionarySetValue(stats, CFSTR("Bytes (Written)"), n_write);
    ::CFRelease(n_read);
    ::CFRelease(n_write);

    common::health_record record;
    backend::parse_macos_statistics(stats, record);
    ::CFRelease(stats);

    expect(record.has_data_units_read_bytes &&
               record.data_units_read_bytes == 10485760ULL,
           "Bytes (Read) must be extracted correctly");
    expect(record.has_data_units_written_bytes &&
               record.data_units_written_bytes == 5242880ULL,
           "Bytes (Written) must be extracted correctly");
}

struct synthetic_health_drive_api {
    static syscape::result<::CFArrayRef> whole_media_facts() {
        static std::vector<::CFDictionaryRef> entries;
        if (entries.empty()) {
            long long size0 = 1000204886016LL;
            long long block_size = 512LL;
            ::CFDictionaryRef d0 = make_media_dictionary(
                "disk0", "NVMe", &size0, &block_size, true, false,
                "APPLE SSD", "Apple", "1.0");
            const ::CFStringRef smart0 = make_string("Verified");
            ::CFDictionarySetValue(const_cast<::CFMutableDictionaryRef>(d0),
                                   CFSTR("SMART Status"), smart0);
            ::CFRelease(smart0);

            ::CFMutableDictionaryRef stats0 = ::CFDictionaryCreateMutable(
                ::kCFAllocatorDefault, 0, &::kCFTypeDictionaryKeyCallBacks,
                &::kCFTypeDictionaryValueCallBacks);
            long long r0 = 10485760LL;
            long long w0 = 5242880LL;
            ::CFNumberRef nr0 = make_number(r0);
            ::CFNumberRef nw0 = make_number(w0);
            ::CFDictionarySetValue(stats0, CFSTR("Bytes (Read)"), nr0);
            ::CFDictionarySetValue(stats0, CFSTR("Bytes (Written)"), nw0);
            ::CFRelease(nr0);
            ::CFRelease(nw0);
            ::CFDictionarySetValue(const_cast<::CFMutableDictionaryRef>(d0),
                                   CFSTR("Statistics"), stats0);
            ::CFRelease(stats0);

            long long size1 = 500107862016LL;
            ::CFDictionaryRef d1 = make_media_dictionary(
                "disk1", "SATA", &size1, &block_size, false, false,
                "Crucial SSD", "Crucial", "M3CR046");
            const ::CFStringRef smart1 = make_string("Failing");
            ::CFDictionarySetValue(const_cast<::CFMutableDictionaryRef>(d1),
                                   CFSTR("SMART Status"), smart1);
            ::CFRelease(smart1);

            long long size2 = 64000000000LL;
            ::CFDictionaryRef d2 = make_media_dictionary(
                "disk2", "USB", &size2, &block_size, true, true,
                "Flash Drive", "SanDisk", "1.0");

            entries.push_back(d0);
            entries.push_back(d1);
            entries.push_back(d2);
        }
        return retain_all(entries);
    }
};

void test_macos_health_collection() {
    namespace backend = syscape::detail::storage_backend;
    namespace common = syscape::detail::storage_common;

    const auto all =
        backend::collect_all_drive_health<synthetic_health_drive_api>();
    expect(all.has_value() && all->size() == 3U,
           "collect_all_drive_health must return 3 drives");
    if (all && all->size() == 3U) {
        const auto& h0 = (*all)[0];
        expect(h0.identifier == "disk0", "First drive must be disk0");
        expect(h0.has_failure_predicted && !h0.failure_predicted,
               "disk0 must have failure_predicted == false");
        expect(h0.status == common::health_status_classification::healthy,
               "disk0 status must be healthy");
        expect(h0.has_data_units_read_bytes &&
                   h0.data_units_read_bytes == 10485760ULL,
               "disk0 read bytes must match");
        expect(h0.has_data_units_written_bytes &&
                   h0.data_units_written_bytes == 5242880ULL,
               "disk0 written bytes must match");

        const auto& h1 = (*all)[1];
        expect(h1.identifier == "disk1", "Second drive must be disk1");
        expect(h1.has_failure_predicted && h1.failure_predicted,
               "disk1 must have failure_predicted == true");
        expect(h1.status == common::health_status_classification::warning,
               "disk1 status must be warning");

        const auto& h2 = (*all)[2];
        expect(h2.identifier == "disk2", "Third drive must be disk2");
        expect(!h2.has_failure_predicted,
               "disk2 without SMART must have has_failure_predicted == false");
        expect(h2.status == common::health_status_classification::unknown,
               "disk2 without SMART must have status unknown");
    }

    const auto single0 =
        backend::collect_drive_health<synthetic_health_drive_api>("disk0");
    expect(single0.has_value() && single0->identifier == "disk0",
           "collect_drive_health for disk0 must succeed");

    const auto single_missing =
        backend::collect_drive_health<synthetic_health_drive_api>("disk99");
    expect(!single_missing && single_missing.error() == syscape::errc::not_found,
           "collect_drive_health for nonexistent drive must return not_found");
}

} // namespace

int main() {
    test_protocol_classification();
    test_media_conversion();
    test_collection();
    test_partition_collection();
    test_smart_status_parsing();
    test_macos_statistics_parsing();
    test_macos_health_collection();
    return failures == 0 ? 0 : 1;
}
