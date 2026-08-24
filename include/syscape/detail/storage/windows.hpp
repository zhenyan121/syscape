#ifndef SYSCAPE_DETAIL_STORAGE_WINDOWS_HPP
#define SYSCAPE_DETAIL_STORAGE_WINDOWS_HPP

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0601
#error "syscape/storage.hpp requires _WIN32_WINNT >= 0x0601 on Windows"
#endif

#if defined(WINVER) && WINVER < 0x0601
#error "syscape/storage.hpp requires WINVER >= 0x0601 on Windows"
#endif

#if !defined(_WIN32_WINNT)
#define SYSCAPE_DETAIL_STORAGE_DEFINED_WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#if !defined(WINVER)
#define SYSCAPE_DETAIL_STORAGE_DEFINED_WINVER
#define WINVER 0x0601
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <windows.h>
#include <setupapi.h>
#include <winioctl.h>

#include <syscape/detail/storage/common.hpp>
#include <syscape/result.hpp>

#if defined(SYSCAPE_DETAIL_STORAGE_DEFINED_WINVER)
#undef WINVER
#undef SYSCAPE_DETAIL_STORAGE_DEFINED_WINVER
#endif

#if defined(SYSCAPE_DETAIL_STORAGE_DEFINED_WIN32_WINNT)
#undef _WIN32_WINNT
#undef SYSCAPE_DETAIL_STORAGE_DEFINED_WIN32_WINNT
#endif

namespace syscape {
namespace detail {
namespace storage_backend {

inline std::error_code last_error() noexcept {
    return std::error_code(static_cast<int>(::GetLastError()),
                           std::system_category());
}

/// Owns one opened device handle for the duration of its queries.
class device_handle {
public:
    explicit device_handle(::HANDLE value) noexcept : value_(value) {}
    device_handle(const device_handle&) = delete;
    device_handle& operator=(const device_handle&) = delete;
    ~device_handle() {
        if (value_ != INVALID_HANDLE_VALUE && value_ != nullptr) {
            static_cast<void>(::CloseHandle(value_));
        }
    }

    ::HANDLE get() const noexcept { return value_; }

private:
    ::HANDLE value_;
};

/// Owns a device-information set returned by SetupAPI.
class device_information_set {
public:
    explicit device_information_set(::HDEVINFO value) noexcept
        : value_(value) {}
    device_information_set(const device_information_set&) = delete;
    device_information_set& operator=(const device_information_set&) = delete;
    ~device_information_set() {
        if (value_ != INVALID_HANDLE_VALUE) {
            static_cast<void>(::SetupDiDestroyDeviceInfoList(value_));
        }
    }

    ::HDEVINFO get() const noexcept { return value_; }

private:
    ::HDEVINFO value_;
};

/// The documented GUID_DEVINTERFACE_DISK value, kept local so including this
/// header does not require a separately defined SDK GUID object.
constexpr ::GUID disk_interface_class = {
    0x53f56307L, 0xb6bf, 0x11d0,
    {0x94U, 0xf2U, 0x00U, 0xa0U, 0xc9U, 0x1eU, 0xfbU, 0x8bU}};

/// Returns whether opening a previously enumerated disk failed because the
/// device disappeared during the query.
inline bool is_drive_population_race(const std::error_code& error) noexcept {
    return error == std::error_code(ERROR_FILE_NOT_FOUND,
                                    std::system_category()) ||
           error == std::error_code(ERROR_PATH_NOT_FOUND,
                                    std::system_category()) ||
           error == std::error_code(ERROR_NO_SUCH_DEVICE,
                                    std::system_category()) ||
           error == std::error_code(ERROR_DEVICE_NOT_CONNECTED,
                                    std::system_category());
}

/// Maps the documented bus-type constants onto the portable transport
/// vocabulary.
///
/// Constants outside this vocabulary, including ones future SDKs add,
/// record unknown instead of failing the enumeration, because a new
/// transport classification describes unfamiliar hardware honestly while a
/// failed query helps nobody.
inline storage_common::bus_classification classify_bus(
    ::STORAGE_BUS_TYPE bus) noexcept {
    using storage_common::bus_classification;
    switch (bus) {
    case BusTypeUnknown:
        return bus_classification::unknown;
    case BusTypeScsi:
        return bus_classification::scsi;
    case BusTypeAtapi:
        return bus_classification::atapi;
    case BusTypeAta:
        return bus_classification::ata;
    case BusType1394:
        return bus_classification::firewire;
    case BusTypeFibreChannel:
        return bus_classification::fibre_channel;
    case BusTypeUsb:
        return bus_classification::usb;
    case BusTypeRaid:
        return bus_classification::raid;
    case BusTypeiScsi:
        return bus_classification::iscsi;
    case BusTypeSas:
        return bus_classification::sas;
    case BusTypeSata:
        return bus_classification::sata;
    case BusTypeSd:
        return bus_classification::sd;
    case BusTypeMmc:
        return bus_classification::mmc;
    case BusTypeVirtual:
    case BusTypeFileBackedVirtual:
        return bus_classification::virtual_media;
    case BusTypeNvme:
        return bus_classification::nvme;
    default:
        return bus_classification::unknown;
    }
}

/// Copies one NUL-terminated ANSI field out of a descriptor buffer.
///
/// An offset of zero records an absent field, exactly as the interface
/// documents optional strings. A field must terminate inside the returned
/// buffer; a truncated rendering cannot be interpreted safely and is
/// malformed platform data. Bytes outside ASCII have no documented meaning
/// and fail the conversion instead of being reinterpreted.
inline result<std::string> descriptor_string(
    const std::vector<char>& buffer, ::DWORD offset) {
    if (offset == 0U) { return std::string(); }
    if (static_cast<std::size_t>(offset) >= buffer.size()) {
        return fail(errc::malformed_data);
    }
    const char* first = buffer.data() + offset;
    const char* last = buffer.data() + buffer.size();
    std::size_t length = 0U;
    while (first + length < last && first[length] != '\0') { ++length; }
    if (first + length == last) {
        return fail(errc::malformed_data);
    }
    std::string text(length, '\0');
    for (std::size_t position = 0U; position < length; ++position) {
        const unsigned char letter =
            static_cast<unsigned char>(first[position]);
        if (letter >= 0x80U) {
            return fail(errc::invalid_encoding);
        }
        text[position] = static_cast<char>(letter);
    }
    while (!text.empty() && text.back() == ' ') { text.pop_back(); }
    return text;
}

/// Converts one StorageDeviceDescriptor record into its portable fields.
///
/// Records shorter than the documented fixed part cannot describe a device
/// and are malformed platform data. The descriptor carries no rotation
/// fact, so callers leave rotation unrecorded instead of inferring it.
inline result<std::pair<bool, storage_common::bus_classification>>
convert_device_descriptor_identity(
    const std::vector<char>& buffer, bool& has_vendor,
    std::string& vendor, bool& has_model, std::string& model,
    bool& has_firmware_revision, std::string& firmware_revision) {
    if (buffer.size() < sizeof(::STORAGE_DEVICE_DESCRIPTOR)) {
        return fail(errc::malformed_data);
    }
    // The record arrives as an untyped byte buffer whose alignment is only
    // that of char, so the fixed part is copied out before field access.
    ::STORAGE_DEVICE_DESCRIPTOR descriptor;
    std::memcpy(&descriptor, buffer.data(), sizeof(descriptor));

    const result<std::string> vendor_text =
        descriptor_string(buffer, descriptor.VendorIdOffset);
    if (!vendor_text) { return fail(vendor_text.error()); }
    has_vendor = !vendor_text->empty();
    vendor = *vendor_text;

    const result<std::string> model_text =
        descriptor_string(buffer, descriptor.ProductIdOffset);
    if (!model_text) { return fail(model_text.error()); }
    has_model = !model_text->empty();
    model = *model_text;

    const result<std::string> revision_text =
        descriptor_string(buffer, descriptor.ProductRevisionOffset);
    if (!revision_text) { return fail(revision_text.error()); }
    has_firmware_revision = !revision_text->empty();
    firmware_revision = *revision_text;

    return std::make_pair(descriptor.RemovableMedia != FALSE,
                          classify_bus(descriptor.BusType));
}

/// Converts one StorageAccessAlignmentDescriptor record into its block
/// sizes.
///
/// A record shorter than the documented fixed part is malformed platform
/// data. Zero block sizes contradict the interface's own addressing rules
/// and are rejected here so the public boundary receives usable facts.
inline result<std::pair<std::uint32_t, std::uint32_t>>
convert_alignment_descriptor(const std::vector<char>& buffer) {
    if (buffer.size() < sizeof(::STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR)) {
        return fail(errc::malformed_data);
    }
    // The record arrives as an untyped byte buffer whose alignment is only
    // that of char, so the fixed part is copied out before field access.
    ::STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR descriptor;
    std::memcpy(&descriptor, buffer.data(), sizeof(descriptor));
    if (descriptor.BytesPerLogicalSector == 0U ||
        descriptor.BytesPerPhysicalSector == 0U) {
        return fail(errc::malformed_data);
    }
    constexpr ::ULONG limit = static_cast<::ULONG>(
        std::numeric_limits<std::uint32_t>::max());
    if (descriptor.BytesPerLogicalSector > limit ||
        descriptor.BytesPerPhysicalSector > limit) {
        return fail(errc::value_too_large);
    }
    return std::make_pair(
        static_cast<std::uint32_t>(descriptor.BytesPerLogicalSector),
        static_cast<std::uint32_t>(descriptor.BytesPerPhysicalSector));
}

/// Converts one DISK_GEOMETRY_EX record into the total disk capacity in
/// bytes.
///
/// The recorded disk size is a signed 64-bit quantity, and a negative
/// recording contradicts the notion of capacity, so such records are
/// malformed platform data rather than wrapped-around values.
inline result<std::uint64_t> convert_geometry_disk_size(
    const std::vector<char>& buffer) {
    if (buffer.size() < sizeof(::DISK_GEOMETRY_EX)) {
        return fail(errc::malformed_data);
    }
    // The record arrives as an untyped byte buffer whose alignment is only
    // that of char, so the fixed part is copied out before field access.
    ::DISK_GEOMETRY_EX geometry;
    std::memcpy(&geometry, buffer.data(), sizeof(geometry));
    if (geometry.DiskSize.QuadPart < 0) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint64_t>(geometry.DiskSize.QuadPart);
}

/// Builds a fully initialized standard storage-property query.
///
/// STORAGE_PROPERTY_QUERY includes an AdditionalParameters byte even for
/// ordinary descriptor queries. Zeroing the complete record prevents
/// indeterminate stack data from becoming driver input.
inline ::STORAGE_PROPERTY_QUERY make_property_query(
    ::STORAGE_PROPERTY_ID property_id) noexcept {
    ::STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = property_id;
    query.QueryType = PropertyStandardQuery;
    return query;
}

/// Platform calls used to enumerate physical drives.
///
/// The indirection exists so tests can drive enumeration with synthetic
/// records instead of real drives; production callers always use the native
/// implementation.
struct native_drive_api {
    /// Enumerates currently present disk device interfaces and obtains each
    /// interface's documented physical device number.
    static result<std::vector<unsigned int>> drive_indices() {
        const ::HDEVINFO raw_set = ::SetupDiGetClassDevsW(
            &disk_interface_class, nullptr, nullptr,
            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (raw_set == INVALID_HANDLE_VALUE) { return fail(last_error()); }
        const device_information_set set(raw_set);

        std::vector<unsigned int> indices;
        for (::DWORD position = 0U;; ++position) {
            ::SP_DEVICE_INTERFACE_DATA interface_data{};
            interface_data.cbSize = sizeof(interface_data);
            if (::SetupDiEnumDeviceInterfaces(
                    set.get(), nullptr, &disk_interface_class, position,
                    &interface_data) == FALSE) {
                const std::error_code error = last_error();
                if (error == std::error_code(ERROR_NO_MORE_ITEMS,
                                             std::system_category())) {
                    break;
                }
                return fail(error);
            }

            ::DWORD required = 0U;
            const ::BOOL sized = ::SetupDiGetDeviceInterfaceDetailW(
                set.get(), &interface_data, nullptr, 0U, &required, nullptr);
            if (sized != FALSE) { return fail(errc::malformed_data); }
            const std::error_code sized_error = last_error();
            if (is_drive_population_race(sized_error)) { continue; }
            if (sized_error != std::error_code(
                                   ERROR_INSUFFICIENT_BUFFER,
                                   std::system_category())) {
                return fail(sized_error);
            }
            if (required < sizeof(::SP_DEVICE_INTERFACE_DETAIL_DATA_W)) {
                return fail(errc::malformed_data);
            }

            const std::size_t units =
                (static_cast<std::size_t>(required) +
                 sizeof(std::max_align_t) - 1U) /
                sizeof(std::max_align_t);
            std::vector<std::max_align_t> detail_storage(units);
            auto* detail =
                reinterpret_cast<::SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(
                    detail_storage.data());
            detail->cbSize = sizeof(*detail);
            if (::SetupDiGetDeviceInterfaceDetailW(
                    set.get(), &interface_data, detail, required, nullptr,
                    nullptr) == FALSE) {
                const std::error_code error = last_error();
                if (is_drive_population_race(error)) { continue; }
                return fail(error);
            }

            ::HANDLE opened = ::CreateFileW(
                detail->DevicePath, 0U,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr, OPEN_EXISTING, 0U, nullptr);
            if (opened == INVALID_HANDLE_VALUE) {
                const std::error_code error = last_error();
                if (is_drive_population_race(error)) { continue; }
                return fail(error);
            }
            const device_handle owned(opened);

            ::STORAGE_DEVICE_NUMBER number{};
            ::DWORD returned = 0U;
            if (::DeviceIoControl(
                    opened, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0U,
                    &number, sizeof(number), &returned, nullptr) == FALSE) {
                const std::error_code error = last_error();
                if (is_drive_population_race(error)) { continue; }
                return fail(error);
            }
            if (returned < sizeof(number) ||
                number.DeviceType != FILE_DEVICE_DISK) {
                return fail(errc::malformed_data);
            }
            indices.push_back(static_cast<unsigned int>(number.DeviceNumber));
        }
        std::sort(indices.begin(), indices.end());
        indices.erase(std::unique(indices.begin(), indices.end()),
                      indices.end());
        return indices;
    }

    /// Opens \\.\PhysicalDriveN with zero desired access, which the storage
    /// stack permits for inquiry queries without administrative rights.
    static result<::HANDLE> open_drive(unsigned int index) {
        const std::wstring path =
            std::wstring(L"\\\\.\\PhysicalDrive") +
            std::to_wstring(index);
        ::HANDLE opened = ::CreateFileW(
            path.c_str(), 0U, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
            OPEN_EXISTING, 0U, nullptr);
        if (opened == INVALID_HANDLE_VALUE) {
            return fail(last_error());
        }
        return opened;
    }

    static void close_drive(::HANDLE handle) noexcept {
        static_cast<void>(::CloseHandle(handle));
    }

    /// Runs one storage-property query through growth-bounded retries.
    static result<std::vector<char>> query_property(
        ::HANDLE handle, ::STORAGE_PROPERTY_ID property_id) {
        ::STORAGE_PROPERTY_QUERY query = make_property_query(property_id);

        std::vector<char> buffer(sizeof(::STORAGE_DESCRIPTOR_HEADER));
        for (;;) {
            ::DWORD returned = 0U;
            const ::BOOL stored = ::DeviceIoControl(
                handle, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
                buffer.data(),
                static_cast<::DWORD>(buffer.size()), &returned, nullptr);
            if (stored != FALSE) {
                if (returned < sizeof(::STORAGE_DESCRIPTOR_HEADER) ||
                    static_cast<std::size_t>(returned) > buffer.size()) {
                    return fail(errc::malformed_data);
                }
                ::STORAGE_DESCRIPTOR_HEADER header;
                std::memcpy(&header, buffer.data(), sizeof(header));
                if (header.Size < sizeof(header)) {
                    return fail(errc::malformed_data);
                }
                if (header.Size > 1024U * 1024U) {
                    return fail(errc::value_too_large);
                }
                if (static_cast<std::size_t>(header.Size) > buffer.size()) {
                    buffer.resize(header.Size);
                    continue;
                }
                if (returned < header.Size) {
                    return fail(errc::malformed_data);
                }
                buffer.resize(returned);
                return buffer;
            }
            const std::error_code error = last_error();
            if (error != std::error_code(ERROR_MORE_DATA,
                                         std::system_category()) &&
                error != std::error_code(ERROR_INSUFFICIENT_BUFFER,
                                         std::system_category())) {
                return fail(error);
            }
            if (buffer.size() >= 1024U * 1024U) {
                return fail(errc::value_too_large);
            }
            buffer.resize(buffer.size() * 2U);
        }
    }

    /// Queries the drive geometry including its total size.
    static result<std::vector<char>> query_geometry(::HANDLE handle) {
        std::vector<char> buffer(sizeof(::DISK_GEOMETRY_EX) + 64U);
        ::DWORD returned = 0U;
        if (::DeviceIoControl(handle, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                              nullptr, 0U, buffer.data(),
                              static_cast<::DWORD>(buffer.size()),
                              &returned, nullptr) == FALSE) {
            return fail(last_error());
        }
        buffer.resize(returned);
        return buffer;
    }
};

/// Owns a handle through the API that produced it. Synthetic tests can use a
/// no-op closer while the native API closes real kernel handles.
template <typename Api>
class api_device_handle {
public:
    api_device_handle(const Api& api, ::HANDLE value) noexcept
        : api_(&api), value_(value) {}
    api_device_handle(const api_device_handle&) = delete;
    api_device_handle& operator=(const api_device_handle&) = delete;
    ~api_device_handle() { api_->close_drive(value_); }

private:
    const Api* api_;
    ::HANDLE value_;
};

/// Builds drive records from the PhysicalDrive indices exposed by the given
/// API.
///
/// A listed index that disappears before it can be opened is skipped as a
/// population race. Indices that cannot be opened for other reasons are also
/// skipped: restrictive policies can deny individual physical-drive handles,
/// and omitting such an entry keeps every readable drive visible instead of
/// discarding them all. Failures on an already-opened handle propagate
/// unchanged because they describe real errors, not population races.
template <typename Api>
result<std::vector<storage_common::drive_record>> enumerate_drives(
    const Api& api) {
    std::vector<storage_common::drive_record> records;
    const result<std::vector<unsigned int>> indices = api.drive_indices();
    if (!indices) { return fail(indices.error()); }
    for (const unsigned int index : *indices) {
        const result<::HANDLE> opened = api.open_drive(index);
        if (!opened) {
            if (is_drive_population_race(opened.error())) { continue; }
            return fail(opened.error());
        }
        const api_device_handle<Api> owned(api, *opened);

        const result<std::vector<char>> descriptor_buffer =
            api.query_property(*opened, StorageDeviceProperty);
        if (!descriptor_buffer) { return fail(descriptor_buffer.error()); }

        storage_common::drive_record record;
        record.identifier = "PhysicalDrive" + std::to_string(index);

        const result<std::pair<bool, storage_common::bus_classification>>
            identity = convert_device_descriptor_identity(
                *descriptor_buffer, record.has_vendor, record.vendor,
                record.has_model, record.model,
                record.has_firmware_revision, record.firmware_revision);
        if (!identity) { return fail(identity.error()); }
        record.removable = identity->first;
        record.bus = identity->second;

        const result<std::vector<char>> alignment_buffer =
            api.query_property(*opened, StorageAccessAlignmentProperty);
        if (alignment_buffer) {
            const result<std::pair<std::uint32_t, std::uint32_t>>
                alignment = convert_alignment_descriptor(*alignment_buffer);
            if (!alignment) { return fail(alignment.error()); }
            record.has_logical_sector_size_bytes = true;
            record.logical_sector_size_bytes = alignment->first;
            record.has_physical_sector_size_bytes = true;
            record.physical_sector_size_bytes = alignment->second;
        } else if (
            alignment_buffer.error() ==
            std::error_code(ERROR_NOT_SUPPORTED, std::system_category())) {
            // Devices behind stacks without access-alignment support stay
            // enumerated with absent sizes, exactly like devices whose
            // sysfs queue attributes Linux omits.
        } else {
            return fail(alignment_buffer.error());
        }

        const result<std::vector<char>> geometry_buffer =
            api.query_geometry(*opened);
        if (!geometry_buffer) { return fail(geometry_buffer.error()); }
        const result<std::uint64_t> capacity =
            convert_geometry_disk_size(*geometry_buffer);
        if (!capacity) { return fail(capacity.error()); }
        record.has_capacity_bytes = true;
        record.capacity_bytes = *capacity;

        records.push_back(std::move(record));
    }
    std::sort(records.begin(), records.end(),
              [](const storage_common::drive_record& left,
                 const storage_common::drive_record& right) {
                  return left.identifier < right.identifier;
              });
    return records;
}

inline result<std::vector<storage_common::drive_record>> drives() {
    return enumerate_drives(native_drive_api{});
}

} // namespace storage_backend
} // namespace detail
} // namespace syscape

#endif
