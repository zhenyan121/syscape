#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <windows.h>
#include <winioctl.h>

#include <syscape/storage.hpp>
#include <syscape/detail/storage/windows.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

/// Builds one StorageDeviceDescriptor byte record whose optional ANSI
/// fields are laid out exactly as the interface documents them.
std::vector<char> make_descriptor(const char* vendor, const char* product,
                                  const char* revision, bool removable,
                                  ::STORAGE_BUS_TYPE bus) {
    ::STORAGE_DEVICE_DESCRIPTOR header;
    std::memset(&header, 0, sizeof(header));

    std::string blob;
    std::size_t cursor = sizeof(header);
    const auto place = [&blob, &cursor](const char* text) -> ::DWORD {
        if (text == nullptr) { return 0U; }
        const ::DWORD offset =
            static_cast<::DWORD>(cursor + blob.size());
        blob.append(text);
        blob.push_back('\0');
        return offset;
    };

    header.RemovableMedia = removable ? TRUE : FALSE;
    header.BusType = bus;
    header.VendorIdOffset = place(vendor);
    header.ProductIdOffset = place(product);
    header.ProductRevisionOffset = place(revision);

    std::vector<char> buffer(sizeof(header) + blob.size());
    std::memcpy(buffer.data(), &header, sizeof(header));
    std::memcpy(buffer.data() + sizeof(header), blob.data(), blob.size());
    return buffer;
}

/// Builds one StorageAccessAlignmentDescriptor byte record.
std::vector<char> make_alignment(::ULONG logical, ::ULONG physical) {
    ::STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR header;
    std::memset(&header, 0, sizeof(header));
    header.BytesPerLogicalSector = logical;
    header.BytesPerPhysicalSector = physical;
    std::vector<char> buffer(sizeof(header));
    std::memcpy(buffer.data(), &header, sizeof(header));
    return buffer;
}

/// Builds one DISK_GEOMETRY_EX byte record carrying only its disk size.
std::vector<char> make_geometry(long long bytes) {
    ::DISK_GEOMETRY_EX header;
    std::memset(&header, 0, sizeof(header));
    header.DiskSize.QuadPart = bytes;
    std::vector<char> buffer(sizeof(header));
    std::memcpy(buffer.data(), &header, sizeof(header));
    return buffer;
}

/// One fabricated physical drive behind the synthetic API.
struct synthetic_drive {
    std::vector<char> device_descriptor;
    /// A present alignment record, an explicit failure, or no attempt.
    bool has_alignment = true;
    std::error_code alignment_error =
        std::error_code(ERROR_NOT_SUPPORTED, std::system_category());
    std::vector<char> alignment_descriptor;
    long long disk_size_bytes = 0;
    bool has_layout = true;
    std::error_code layout_error =
        std::error_code(ERROR_NOT_SUPPORTED, std::system_category());
    std::vector<char> layout_descriptor;
};

/// Replays fabricated drives instead of calling the storage stack.
struct synthetic_api {
    std::map<unsigned int, synthetic_drive> drives;
    unsigned int denied_index = ~0U;
    unsigned int removed_index = ~0U;

    syscape::result<std::vector<unsigned int>> drive_indices() const {
        std::vector<unsigned int> indices;
        for (const auto& drive : drives) { indices.push_back(drive.first); }
        if (denied_index != ~0U) { indices.push_back(denied_index); }
        if (removed_index != ~0U) { indices.push_back(removed_index); }
        return indices;
    }

    syscape::result<::HANDLE> open_drive(unsigned int index) const {
        if (index == denied_index) {
            return syscape::fail(std::error_code(
                ERROR_ACCESS_DENIED, std::system_category()));
        }
        if (index == removed_index) {
            return syscape::fail(std::error_code(
                ERROR_DEVICE_NOT_CONNECTED, std::system_category()));
        }
        if (drives.find(index) == drives.end()) {
            return syscape::fail(std::error_code(
                ERROR_FILE_NOT_FOUND, std::system_category()));
        }
        return reinterpret_cast<::HANDLE>(
            static_cast<std::intptr_t>(index + 1));
    }

    static void close_drive(::HANDLE) noexcept {}

    syscape::result<std::vector<char>> query_property(
        ::HANDLE handle, ::STORAGE_PROPERTY_ID property_id) const {
        const unsigned int index =
            static_cast<unsigned int>(
                reinterpret_cast<std::intptr_t>(handle)) - 1U;
        const synthetic_drive& drive = drives.at(index);
        if (property_id == StorageDeviceProperty) {
            return drive.device_descriptor;
        }
        if (!drive.has_alignment) {
            return syscape::fail(drive.alignment_error);
        }
        return drive.alignment_descriptor;
    }

    syscape::result<std::vector<char>> query_geometry(
        ::HANDLE handle) const {
        const unsigned int index =
            static_cast<unsigned int>(
                reinterpret_cast<std::intptr_t>(handle)) - 1U;
        return make_geometry(drives.at(index).disk_size_bytes);
    }

    syscape::result<std::vector<char>> query_layout(
        ::HANDLE handle) const {
        const unsigned int index =
            static_cast<unsigned int>(
                reinterpret_cast<std::intptr_t>(handle)) - 1U;
        const synthetic_drive& drive = drives.at(index);
        if (!drive.has_layout) {
            return syscape::fail(drive.layout_error);
        }
        return drive.layout_descriptor;
    }

    std::vector<syscape::detail::storage_backend::volume_extent_record> extents;

    syscape::result<std::vector<syscape::detail::storage_backend::volume_extent_record>>
    volume_extents() const {
        return extents;
    }
};

/// Builds one synthetic MBR layout buffer with one partition.
std::vector<char> make_mbr_layout(long long offset, long long length,
                                  unsigned long part_num,
                                  unsigned char part_type, bool bootable) {
    const std::size_t size = sizeof(::DRIVE_LAYOUT_INFORMATION_EX) +
                             sizeof(::PARTITION_INFORMATION_EX);
    std::vector<char> buffer(size, 0);
    auto* layout =
        reinterpret_cast<::DRIVE_LAYOUT_INFORMATION_EX*>(buffer.data());
    layout->PartitionStyle = PARTITION_STYLE_MBR;
    layout->PartitionCount = 1U;
    layout->Mbr.Signature = 0x12345678U;

    auto* part = reinterpret_cast<::PARTITION_INFORMATION_EX*>(
        buffer.data() + offsetof(::DRIVE_LAYOUT_INFORMATION_EX,
                                 PartitionEntry));
    part->PartitionStyle = PARTITION_STYLE_MBR;
    part->StartingOffset.QuadPart = offset;
    part->PartitionLength.QuadPart = length;
    part->PartitionNumber = part_num;
    part->RewritePartition = FALSE;
    part->Mbr.PartitionType = part_type;
    part->Mbr.BootIndicator = bootable ? TRUE : FALSE;
    part->Mbr.RecognizedPartition = TRUE;
    part->Mbr.HiddenSectors = 0U;
    return buffer;
}

/// Builds one synthetic GPT layout buffer with one partition.
std::vector<char> make_gpt_layout(long long offset, long long length,
                                  unsigned long part_num,
                                  const ::GUID& type_guid,
                                  const ::GUID& id_guid,
                                  const wchar_t* name) {
    const std::size_t size = sizeof(::DRIVE_LAYOUT_INFORMATION_EX) +
                             sizeof(::PARTITION_INFORMATION_EX);
    std::vector<char> buffer(size, 0);
    auto* layout =
        reinterpret_cast<::DRIVE_LAYOUT_INFORMATION_EX*>(buffer.data());
    layout->PartitionStyle = PARTITION_STYLE_GPT;
    layout->PartitionCount = 1U;
    layout->Gpt.DiskId = id_guid;

    auto* part = reinterpret_cast<::PARTITION_INFORMATION_EX*>(
        buffer.data() + offsetof(::DRIVE_LAYOUT_INFORMATION_EX,
                                 PartitionEntry));
    part->PartitionStyle = PARTITION_STYLE_GPT;
    part->StartingOffset.QuadPart = offset;
    part->PartitionLength.QuadPart = length;
    part->PartitionNumber = part_num;
    part->Gpt.PartitionType = type_guid;
    part->Gpt.PartitionId = id_guid;
    part->Gpt.Attributes = 0U;
    if (name != nullptr) {
        for (std::size_t i = 0; i < 36 && name[i] != L'\0'; ++i) {
            part->Gpt.Name[i] = name[i];
        }
    }
    return buffer;
}

void test_guid_formatting() {
    namespace backend = syscape::detail::storage_backend;
    const ::GUID test_guid = {
        0x6dc136bcL, 0x42c9, 0x40ab,
        {0xb8U, 0x50U, 0x6eU, 0x74U, 0xedU, 0xa9U, 0x7dU, 0xd6U}};
    const std::string formatted = backend::format_guid(test_guid);
    expect(formatted == "6dc136bc-42c9-40ab-b850-6e74eda97dd6",
           "GUID formatting must produce standard 36-char lowercase string");
}

void test_volume_extent_conversion() {
    namespace backend = syscape::detail::storage_backend;
    const std::size_t offset = offsetof(::VOLUME_DISK_EXTENTS, Extents);
    std::vector<char> buffer(offset + sizeof(::DISK_EXTENT), 0);
    const ::DWORD count = 1U;
    std::memcpy(buffer.data() +
                    offsetof(::VOLUME_DISK_EXTENTS, NumberOfDiskExtents),
                &count, sizeof(count));
    ::DISK_EXTENT extent{};
    extent.DiskNumber = 3U;
    extent.StartingOffset.QuadPart = 4096LL;
    extent.ExtentLength.QuadPart = 8192LL;
    std::memcpy(buffer.data() + offset, &extent, sizeof(extent));

    const auto converted =
        backend::convert_volume_extents(buffer, "C:\\", "NTFS");
    expect(converted && converted->size() == 1U &&
               (*converted)[0].disk_number == 3U &&
               (*converted)[0].start_offset_bytes == 4096ULL,
           "A complete extent buffer must convert exactly");

    std::vector<char> truncated(buffer.begin(), buffer.end() - 1);
    const auto short_result =
        backend::convert_volume_extents(truncated, "C:\\", "NTFS");
    expect(!short_result &&
               short_result.error() == syscape::errc::malformed_data,
           "A truncated extent array must report malformed_data");

    extent.StartingOffset.QuadPart = -1LL;
    std::memcpy(buffer.data() + offset, &extent, sizeof(extent));
    const auto negative =
        backend::convert_volume_extents(buffer, "C:\\", "NTFS");
    expect(!negative && negative.error() == syscape::errc::malformed_data,
           "A negative volume extent offset must report malformed_data");
}

void test_partition_enumeration() {
    namespace backend = syscape::detail::storage_backend;

    synthetic_api api;
    const ::GUID type_guid = {
        0xc12a7328L, 0xf81f, 0x11d2,
        {0xbaU, 0x4bU, 0x00U, 0xa0U, 0xc9U, 0x3eU, 0xc9U, 0x3bU}};
    const ::GUID id_guid = {
        0x6dc136bcL, 0x42c9, 0x40ab,
        {0xb8U, 0x50U, 0x6eU, 0x74U, 0xedU, 0xa9U, 0x7dU, 0xd6U}};

    // GPT with Chinese characters in partition name
    api.drives[0].layout_descriptor =
        make_gpt_layout(1048576LL, 536870912LL, 1U, type_guid, id_guid,
                        L"系统保留 (System Reserved)");
    api.drives[1].layout_descriptor =
        make_mbr_layout(2097152LL, 1073741824LL, 1U, 0x07U, true);

    // Mock volume extents: PhysicalDrive0 Partition1 -> C:\ (NTFS)
    backend::volume_extent_record ext0;
    ext0.disk_number = 0U;
    ext0.start_offset_bytes = 1048576ULL;
    ext0.mount_point = "C:\\";
    ext0.filesystem_type = "NTFS";
    api.extents.push_back(ext0);

    const auto partitions_result = backend::enumerate_partitions(api);
    expect(partitions_result.has_value(),
           "Synthetic partition enumeration must succeed");
    if (!partitions_result) { return; }

    expect(partitions_result->size() == 2U,
           "Both partitions must be enumerated across the two drives");
    if (partitions_result->size() == 2U) {
        const auto& p0 = (*partitions_result)[0];
        expect(p0.identifier == "PhysicalDrive0Partition1",
               "Partition 0 identifier must match");
        expect(p0.disk_identifier == "PhysicalDrive0",
               "Partition 0 parent disk must match");
        expect(p0.scheme == syscape::storage::partition_scheme::gpt,
               "Partition 0 scheme must be GPT");
        expect(p0.has_name && p0.name == "系统保留 (System Reserved)",
               "Partition 0 non-ASCII UTF-16 name must convert accurately to UTF-8");
        expect(p0.has_uuid &&
                   p0.uuid == "6dc136bc-42c9-40ab-b850-6e74eda97dd6",
               "Partition 0 UUID must match");
        expect(p0.has_size_bytes && p0.size_bytes == 536870912ULL,
               "Partition 0 size must match");
        expect(p0.is_mounted,
               "Partition 0 must report mounted");
        expect(p0.mount_point == "C:\\",
               "Partition 0 mount point must match");
        expect(p0.has_filesystem_type && p0.filesystem_type == "NTFS",
               "Partition 0 filesystem type must match NTFS");

        const auto& p1 = (*partitions_result)[1];
        expect(p1.identifier == "PhysicalDrive1Partition1",
               "Partition 1 identifier must match");
        expect(p1.scheme == syscape::storage::partition_scheme::mbr,
               "Partition 1 scheme must be MBR");
        expect(p1.is_bootable,
               "Partition 1 must be bootable");
        expect(p1.has_type_identifier && p1.type_identifier == "0x07",
               "Partition 1 type identifier must match MBR type 0x07");
        expect(!p1.is_mounted,
               "Partition 1 with no extents must report unmounted");
    }

    // Boundary error tests for convert_drive_layout
    const auto short_layout = backend::convert_drive_layout(
        std::vector<char>(sizeof(::DRIVE_LAYOUT_INFORMATION_EX) - 1U, 0), 0U);
    expect(!short_layout && short_layout.error() == syscape::errc::malformed_data,
           "A layout buffer shorter than DRIVE_LAYOUT_INFORMATION_EX must fail");

    std::vector<char> gpt_buf =
        make_gpt_layout(1048576LL, 536870912LL, 1U, type_guid, id_guid, L"Test");
    auto* layout =
        reinterpret_cast<::DRIVE_LAYOUT_INFORMATION_EX*>(gpt_buf.data());

    layout->PartitionCount = 5U;
    const auto truncated_parts = backend::convert_drive_layout(gpt_buf, 0U);
    expect(!truncated_parts &&
               truncated_parts.error() == syscape::errc::malformed_data,
           "A buffer with fewer partition entries than PartitionCount must fail");

    layout->PartitionCount = 0xFFFFFFFFU;
    const auto overflow_parts = backend::convert_drive_layout(gpt_buf, 0U);
    expect(!overflow_parts &&
               overflow_parts.error() == syscape::errc::malformed_data,
           "A layout with an overflowing PartitionCount must fail");
}

void test_device_descriptor_conversion() {
    namespace backend = syscape::detail::storage_backend;

    bool has_vendor = false;
    std::string vendor;
    bool has_model = false;
    std::string model;
    bool has_revision = false;
    std::string revision;

    const auto identity = backend::convert_device_descriptor_identity(
        make_descriptor("Vendor X ", "Model Y", "rev1", TRUE, BusTypeSata),
        has_vendor, vendor, has_model, model, has_revision, revision);
    expect(identity.has_value(),
           "A well-formed descriptor must convert");
    expect(has_vendor && vendor == "Vendor X",
           "Vendor padding must be trimmed away");
    expect(has_model && model == "Model Y",
           "The product identifier must become the model field");
    expect(has_revision && revision == "rev1",
           "The product revision must become the firmware field");
    expect(identity->first, "A set removability bit must report removable");
    expect(identity->second ==
               syscape::storage::bus_type::sata,
           "The documented SATA constant must map onto sata");

    const auto anonymous = backend::convert_device_descriptor_identity(
        make_descriptor(nullptr, nullptr, nullptr, FALSE, BusTypeNvme),
        has_vendor, vendor, has_model, model, has_revision, revision);
    expect(anonymous.has_value(),
           "A descriptor without any string offsets must convert");
    expect(!has_vendor && !has_model && !has_revision,
           "Absent offsets must leave every text field unrecorded");
    expect(!anonymous->first,
           "A clear removability bit must report fixed media");
    expect(anonymous->second == syscape::storage::bus_type::nvme,
           "The documented NVMe constant must map onto nvme");

    const auto virtual_disk = backend::convert_device_descriptor_identity(
        make_descriptor(nullptr, nullptr, nullptr, FALSE,
                        BusTypeFileBackedVirtual),
        has_vendor, vendor, has_model, model, has_revision, revision);
    expect(virtual_disk.has_value() &&
               virtual_disk->second ==
                   syscape::storage::bus_type::virtual_media,
           "A file-backed virtual bus must map onto virtual_media");

    std::vector<char> padded = make_descriptor("   ", nullptr, nullptr,
                                               FALSE, BusTypeUnknown);
    const auto blank = backend::convert_device_descriptor_identity(
        padded, has_vendor, vendor, has_model, model, has_revision,
        revision);
    expect(blank.has_value() && !has_vendor,
           "A wholly blank vendor string must record an absent field");

    std::vector<char> foreign_bus =
        make_descriptor("v", nullptr, nullptr, FALSE,
                        static_cast<::STORAGE_BUS_TYPE>(0x55U));
    const auto unmapped = backend::convert_device_descriptor_identity(
        foreign_bus, has_vendor, vendor, has_model, model, has_revision,
        revision);
    expect(unmapped.has_value() &&
               unmapped->second == syscape::storage::bus_type::unknown,
           "Bus constants outside the vocabulary must stay unknown");

    std::vector<char> truncated(4U);
    const auto short_record = backend::convert_device_descriptor_identity(
        truncated, has_vendor, vendor, has_model, model, has_revision,
        revision);
    expect(!short_record &&
               short_record.error() == syscape::errc::malformed_data,
           "A record shorter than the fixed part must be malformed data");

    std::vector<char> unterminated(sizeof(::STORAGE_DEVICE_DESCRIPTOR) +
                                   6U);
    {
        ::STORAGE_DEVICE_DESCRIPTOR cut_header;
        std::memset(&cut_header, 0, sizeof(cut_header));
        cut_header.BusType = BusTypeUnknown;
        cut_header.VendorIdOffset =
            static_cast<::DWORD>(sizeof(::STORAGE_DEVICE_DESCRIPTOR));
        std::memcpy(unterminated.data(), &cut_header, sizeof(cut_header));
        std::memcpy(unterminated.data() + sizeof(cut_header), "Vendor", 6U);
    }
    const auto cut = backend::convert_device_descriptor_identity(
        unterminated, has_vendor, vendor, has_model, model, has_revision,
        revision);
    expect(!cut && cut.error() == syscape::errc::malformed_data,
           "A string field that never terminates inside the record cannot "
           "be interpreted safely and must be malformed platform data");

    std::vector<char> high_byte =
        make_descriptor("\xc3", nullptr, nullptr, FALSE, BusTypeUnknown);
    const auto unconvertible = backend::convert_device_descriptor_identity(
        high_byte, has_vendor, vendor, has_model, model, has_revision,
        revision);
    expect(!unconvertible &&
               unconvertible.error() == syscape::errc::invalid_encoding,
           "Bytes outside ASCII have no documented meaning and must fail "
           "the conversion");
}

void test_alignment_conversion() {
    namespace backend = syscape::detail::storage_backend;

    const auto aligned = backend::convert_alignment_descriptor(
        make_alignment(512U, 4096U));
    expect(aligned.has_value() && aligned->first == 512U &&
               aligned->second == 4096U,
           "A documented alignment record must supply both block sizes");

    const auto short_record =
        backend::convert_alignment_descriptor(std::vector<char>(4U));
    expect(!short_record &&
               short_record.error() == syscape::errc::malformed_data,
           "A truncated alignment record must be malformed platform data");

    const auto empty_blocks =
        backend::convert_alignment_descriptor(make_alignment(0U, 4096U));
    expect(!empty_blocks &&
               empty_blocks.error() == syscape::errc::malformed_data,
           "A zero block size contradicts the addressing rules and must "
           "be rejected before the public boundary");
}

void test_geometry_conversion() {
    namespace backend = syscape::detail::storage_backend;

    const auto sized = backend::convert_geometry_disk_size(
        make_geometry(500107862016LL));
    expect(sized.has_value() && *sized == 500107862016ULL,
           "The recorded disk size must pass through unchanged");

    const auto negative =
        backend::convert_geometry_disk_size(make_geometry(-1LL));
    expect(!negative &&
               negative.error() == syscape::errc::malformed_data,
           "A negative capacity recording contradicts the notion of size");

    const auto short_record =
        backend::convert_geometry_disk_size(std::vector<char>(8U));
    expect(!short_record &&
               short_record.error() == syscape::errc::malformed_data,
           "A truncated geometry record must be malformed platform data");
}

void test_property_query_initialization() {
    namespace backend = syscape::detail::storage_backend;

    const ::STORAGE_PROPERTY_QUERY query =
        backend::make_property_query(StorageDeviceProperty);
    expect(query.PropertyId == StorageDeviceProperty &&
               query.QueryType == PropertyStandardQuery,
           "A property query must carry the requested standard property");
    expect(query.AdditionalParameters[0] == 0U,
           "Unused property-query parameters must be zero initialized");
}

void test_enumeration() {
    namespace backend = syscape::detail::storage_backend;

    synthetic_api api;
    api.drives[0].device_descriptor = make_descriptor(
        "Vendor A", "Disk Zero", "fw0", FALSE, BusTypeSata);
    api.drives[0].alignment_descriptor = make_alignment(512U, 4096U);
    api.drives[0].disk_size_bytes = 500107862016LL;
    api.drives[1].device_descriptor = make_descriptor(
        nullptr, "Disk One", nullptr, TRUE, BusTypeUsb);
    api.drives[1].has_alignment = false;
    api.drives[1].disk_size_bytes = 30055924736LL;

    const syscape::result<std::vector<syscape::detail::storage_common::drive_record>>
        listed = backend::enumerate_drives(api);
    expect(listed.has_value(),
           "A synthetic two-drive machine must enumerate successfully");
    if (!listed) { return; }

    expect(listed->size() == 2U,
           "Every listed drive index must be enumerated");
    if (listed->size() != 2U) { return; }

    expect((*listed)[0].identifier == "PhysicalDrive0" &&
               (*listed)[1].identifier == "PhysicalDrive1",
           "Identifiers must render the platform's own drive naming");

    expect((*listed)[0].has_capacity_bytes &&
               (*listed)[0].capacity_bytes == 500107862016ULL,
           "Geometry sizes must surface as byte capacities");
    expect((*listed)[0].has_logical_sector_size_bytes &&
               (*listed)[0].logical_sector_size_bytes == 512U,
           "Alignment records must surface as logical block sizes");
    expect((*listed)[1].bus == syscape::storage::bus_type::usb,
           "Fabricated bus constants must map through the classifier");
    expect((*listed)[1].removable,
           "Fabricated removability bits must pass through");

    expect(!(*listed)[1].has_logical_sector_size_bytes &&
               !(*listed)[1].has_physical_sector_size_bytes,
           "A stack that reports no alignment support must leave both "
           "sizes unrecorded instead of failing the drive");

    synthetic_api gapped;
    gapped.drives[0].device_descriptor =
        make_descriptor(nullptr, "Before gap", nullptr, FALSE, BusTypeScsi);
    gapped.drives[0].alignment_descriptor = make_alignment(512U, 512U);
    gapped.drives[0].disk_size_bytes = 1024LL * 1024LL;
    gapped.drives[2].device_descriptor =
        make_descriptor(nullptr, "After gap", nullptr, FALSE, BusTypeScsi);
    gapped.drives[2].alignment_descriptor = make_alignment(512U, 512U);
    gapped.drives[2].disk_size_bytes = 2048LL * 1024LL;
    gapped.drives[10].device_descriptor =
        make_descriptor(nullptr, "Lexical order", nullptr, FALSE,
                        BusTypeScsi);
    gapped.drives[10].alignment_descriptor = make_alignment(512U, 512U);
    gapped.drives[10].disk_size_bytes = 4096LL * 1024LL;

    const auto gap_preserved = backend::enumerate_drives(gapped);
    expect(gap_preserved.has_value() && gap_preserved->size() == 3U,
           "An absent disk number must not hide later physical drives");
    if (gap_preserved && gap_preserved->size() == 3U) {
        expect((*gap_preserved)[0].identifier == "PhysicalDrive0" &&
                   (*gap_preserved)[1].identifier == "PhysicalDrive10" &&
                   (*gap_preserved)[2].identifier == "PhysicalDrive2",
               "Gapped drives must retain their names and sort by "
               "identifier");
    }

    synthetic_api denied;
    denied.denied_index = 1U;
    denied.drives[0].device_descriptor =
        make_descriptor(nullptr, "Kept", nullptr, FALSE, BusTypeScsi);
    denied.drives[0].alignment_descriptor =
        make_alignment(512U, 512U);
    denied.drives[0].disk_size_bytes = 1024LL * 1024LL;
    denied.drives[2].device_descriptor =
        make_descriptor(nullptr, "Also kept", nullptr, FALSE, BusTypeScsi);
    denied.drives[2].alignment_descriptor =
        make_alignment(512U, 512U);
    denied.drives[2].disk_size_bytes = 2048LL * 1024LL;

    const auto denied_result = backend::enumerate_drives(denied);
    expect(!denied_result &&
               denied_result.error() ==
                   std::error_code(ERROR_ACCESS_DENIED,
                                   std::system_category()),
           "A denied physical-drive handle must report the native error "
           "instead of returning a partial snapshot");

    synthetic_api removed;
    removed.removed_index = 1U;
    removed.drives[0].device_descriptor =
        make_descriptor(nullptr, "Survives", nullptr, FALSE, BusTypeScsi);
    removed.drives[0].alignment_descriptor = make_alignment(512U, 512U);
    removed.drives[0].disk_size_bytes = 1024LL * 1024LL;
    const auto removal_result = backend::enumerate_drives(removed);
    expect(removal_result.has_value() && removal_result->size() == 1U,
           "A drive removed after interface enumeration must be skipped as "
           "a population race");

    synthetic_api broken;
    broken.drives[0].device_descriptor = std::vector<char>(4U);

    const syscape::result<std::vector<syscape::detail::storage_common::drive_record>>
        unusable = backend::enumerate_drives(broken);
    expect(!unusable &&
               unusable.error() == syscape::errc::malformed_data,
           "A structurally unusable descriptor record must fail the "
           "snapshot honestly");

    const synthetic_api none;
    const syscape::result<std::vector<syscape::detail::storage_common::drive_record>>
        empty = backend::enumerate_drives(none);
    expect(empty.has_value() && empty->empty(),
           "A machine without drives enumerates an empty list, which is "
           "valid data");
}

} // namespace

int main() {
    test_device_descriptor_conversion();
    test_alignment_conversion();
    test_geometry_conversion();
    test_property_query_initialization();
    test_enumeration();
    test_guid_formatting();
    test_volume_extent_conversion();
    test_partition_enumeration();
    return failures == 0 ? 0 : 1;
}
