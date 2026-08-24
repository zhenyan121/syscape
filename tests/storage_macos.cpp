#include <cstdint>
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

} // namespace

int main() {
    test_protocol_classification();
    test_media_conversion();
    test_collection();
    return failures == 0 ? 0 : 1;
}
