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
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <windows.h>
#include <setupapi.h>
#include <winioctl.h>

#include <syscape/detail/storage/common.hpp>
#include <syscape/detail/utf8.hpp>
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
    case BusTypeFibre:
        return bus_classification::fibre_channel;
    case BusTypeUsb:
        return bus_classification::usb;
    case BusTypeRAID:
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
        (std::numeric_limits<std::uint32_t>::max)());
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

/// One volume-to-disk correlation entry awaiting partition matching.
struct volume_extent_record {
    unsigned int disk_number = 0U;
    std::uint64_t start_offset_bytes = 0U;
    std::string mount_point;
    std::string filesystem_type;
};

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

    /// Queries the drive partition layout.
    static result<std::vector<char>> query_layout(::HANDLE handle) {
        std::vector<char> buffer(sizeof(::DRIVE_LAYOUT_INFORMATION_EX) +
                                 16U * sizeof(::PARTITION_INFORMATION_EX));
        for (;;) {
            ::DWORD returned = 0U;
            if (::DeviceIoControl(handle, IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
                                  nullptr, 0U, buffer.data(),
                                  static_cast<::DWORD>(buffer.size()),
                                  &returned, nullptr) != FALSE) {
                buffer.resize(returned);
                return buffer;
            }
            const std::error_code error = last_error();
            if (error != std::error_code(ERROR_INSUFFICIENT_BUFFER,
                                         std::system_category()) &&
                error != std::error_code(ERROR_MORE_DATA,
                                         std::system_category())) {
                return fail(error);
            }
            if (buffer.size() >= 4U * 1024U * 1024U) {
                return fail(errc::value_too_large);
            }
            buffer.resize(buffer.size() * 2U);
        }
    }

    /// Queries failure prediction from the storage stack.
    static result<std::vector<char>> query_predict_failure(::HANDLE handle) {
        std::vector<char> buffer(sizeof(::STORAGE_PREDICT_FAILURE));
        ::DWORD returned = 0U;
        if (::DeviceIoControl(handle, IOCTL_STORAGE_PREDICT_FAILURE,
                              nullptr, 0U, buffer.data(),
                              static_cast<::DWORD>(buffer.size()),
                              &returned, nullptr) == FALSE) {
            return fail(last_error());
        }
        buffer.resize(returned);
        return buffer;
    }

    /// Queries logical volume extents, mount points, and filesystem types.
    static result<std::vector<volume_extent_record>> volume_extents();
};

/// Converts a 16-bit wchar_t string view to UTF-8 using strict encoding rules.
inline result<std::string> wide_to_utf8(std::wstring_view value) {
    static_assert(sizeof(wchar_t) == sizeof(char16_t),
                  "The Windows backend requires 16-bit wchar_t");
    std::u16string converted;
    converted.reserve(value.size());
    for (wchar_t unit : value) {
        converted.push_back(static_cast<char16_t>(unit));
    }
    return utf16_to_utf8(converted);
}

/// Converts one validated VOLUME_DISK_EXTENTS buffer into portable records.
inline result<std::vector<volume_extent_record>> convert_volume_extents(
    const std::vector<char>& buffer, const std::string& mount_point,
    const std::string& filesystem_type) {
    constexpr std::size_t extent_offset =
        offsetof(::VOLUME_DISK_EXTENTS, Extents);
    if (buffer.size() < extent_offset) {
        return fail(errc::malformed_data);
    }
    ::DWORD count = 0U;
    std::memcpy(&count,
                buffer.data() + offsetof(::VOLUME_DISK_EXTENTS,
                                         NumberOfDiskExtents),
                sizeof(count));
    if (static_cast<std::size_t>(count) >
        ((std::numeric_limits<std::size_t>::max)() - extent_offset) /
            sizeof(::DISK_EXTENT)) {
        return fail(errc::value_too_large);
    }
    const std::size_t required =
        extent_offset +
        static_cast<std::size_t>(count) * sizeof(::DISK_EXTENT);
    if (buffer.size() < required) { return fail(errc::malformed_data); }

    std::vector<volume_extent_record> records;
    records.reserve(count);
    for (::DWORD i = 0U; i < count; ++i) {
        ::DISK_EXTENT extent;
        std::memcpy(&extent,
                    buffer.data() + extent_offset +
                        static_cast<std::size_t>(i) * sizeof(extent),
                    sizeof(extent));
        if (extent.StartingOffset.QuadPart < 0 ||
            extent.ExtentLength.QuadPart <= 0) {
            return fail(errc::malformed_data);
        }
        volume_extent_record record;
        record.disk_number = extent.DiskNumber;
        record.start_offset_bytes =
            static_cast<std::uint64_t>(extent.StartingOffset.QuadPart);
        record.mount_point = mount_point;
        record.filesystem_type = filesystem_type;
        records.push_back(std::move(record));
    }
    return records;
}

inline result<std::vector<volume_extent_record>>
native_drive_api::volume_extents() {
    class find_volume_handle {
    public:
        explicit find_volume_handle(::HANDLE value) noexcept : value_(value) {}
        find_volume_handle(const find_volume_handle&) = delete;
        find_volume_handle& operator=(const find_volume_handle&) = delete;
        ~find_volume_handle() {
            if (value_ != INVALID_HANDLE_VALUE) {
                static_cast<void>(::FindVolumeClose(value_));
            }
        }

    private:
        ::HANDLE value_;
    };

    const auto query_paths = [](const std::wstring& volume)
        -> result<std::vector<std::wstring>> {
        ::DWORD capacity = 256U;
        constexpr ::DWORD maximum_characters = 32768U;
        for (;;) {
            std::vector<wchar_t> buffer(capacity, L'\0');
            ::DWORD required = 0U;
            if (::GetVolumePathNamesForVolumeNameW(
                    volume.c_str(), buffer.data(), capacity, &required) !=
                FALSE) {
                std::vector<std::wstring> paths;
                std::size_t offset = 0U;
                while (offset < buffer.size() && buffer[offset] != L'\0') {
                    std::size_t end = offset;
                    while (end < buffer.size() && buffer[end] != L'\0') {
                        ++end;
                    }
                    if (end == buffer.size()) {
                        return fail(errc::malformed_data);
                    }
                    paths.emplace_back(buffer.data() + offset, end - offset);
                    offset = end + 1U;
                }
                std::sort(paths.begin(), paths.end());
                return paths;
            }
            const std::error_code error = last_error();
            if (error != std::error_code(ERROR_MORE_DATA,
                                         std::system_category())) {
                return fail(error);
            }
            if (required <= capacity || required > maximum_characters) {
                if (required > maximum_characters) {
                    return fail(errc::value_too_large);
                }
                return fail(errc::malformed_data);
            }
            capacity = required;
        }
    };

    const auto query_extent_buffer = [](::HANDLE handle)
        -> result<std::vector<char>> {
        std::vector<char> buffer(sizeof(::VOLUME_DISK_EXTENTS) +
                                 8U * sizeof(::DISK_EXTENT));
        for (;;) {
            ::DWORD returned = 0U;
            if (::DeviceIoControl(
                    handle, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0U,
                    buffer.data(), static_cast<::DWORD>(buffer.size()),
                    &returned, nullptr) != FALSE) {
                if (static_cast<std::size_t>(returned) > buffer.size()) {
                    return fail(errc::malformed_data);
                }
                buffer.resize(returned);
                return buffer;
            }
            const std::error_code error = last_error();
            if (error != std::error_code(ERROR_INSUFFICIENT_BUFFER,
                                         std::system_category()) &&
                error != std::error_code(ERROR_MORE_DATA,
                                         std::system_category())) {
                return fail(error);
            }
            if (buffer.size() >= 1024U * 1024U) {
                return fail(errc::value_too_large);
            }
            buffer.resize(buffer.size() * 2U);
        }
    };

    std::vector<volume_extent_record> extents_list;
    std::vector<wchar_t> volume_buffer(1024U, L'\0');
    const ::HANDLE raw_find = ::FindFirstVolumeW(
        volume_buffer.data(), static_cast<::DWORD>(volume_buffer.size()));
    if (raw_find == INVALID_HANDLE_VALUE) {
        const std::error_code error = last_error();
        if (error == std::error_code(ERROR_NO_MORE_FILES,
                                     std::system_category())) {
            return extents_list;
        }
        return fail(error);
    }
    const find_volume_handle owned_find(raw_find);

    for (;;) {
        std::size_t volume_length = 0U;
        while (volume_length < volume_buffer.size() &&
               volume_buffer[volume_length] != L'\0') {
            ++volume_length;
        }
        if (volume_length == 0U || volume_length == volume_buffer.size() ||
            volume_buffer[volume_length - 1U] != L'\\') {
            return fail(errc::malformed_data);
        }
        const std::wstring volume(volume_buffer.data(), volume_length);
        std::wstring open_name(volume, 0U, volume.size() - 1U);
        const ::HANDLE raw_volume = ::CreateFileW(
            open_name.c_str(), 0U, FILE_SHARE_READ | FILE_SHARE_WRITE |
                                    FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, 0U, nullptr);
        if (raw_volume == INVALID_HANDLE_VALUE) {
            const std::error_code error = last_error();
            if (!is_drive_population_race(error)) { return fail(error); }
        } else {
            const device_handle owned_volume(raw_volume);
            const result<std::vector<char>> extent_buffer =
                query_extent_buffer(raw_volume);
            if (!extent_buffer) {
                if (extent_buffer.error() !=
                        std::error_code(ERROR_INVALID_FUNCTION,
                                        std::system_category()) &&
                    extent_buffer.error() !=
                        std::error_code(ERROR_NOT_SUPPORTED,
                                        std::system_category())) {
                    return fail(extent_buffer.error());
                }
            } else {
                const result<std::vector<std::wstring>> paths =
                    query_paths(volume);
                if (!paths) { return fail(paths.error()); }
                std::string mount_point;
                if (!paths->empty()) {
                    const result<std::string> converted_path =
                        wide_to_utf8(paths->front());
                    if (!converted_path) {
                        return fail(converted_path.error());
                    }
                    mount_point = *converted_path;
                }

                std::string filesystem_type;
                wchar_t fs_name[256] = {};
                if (::GetVolumeInformationW(
                        volume.c_str(), nullptr, 0U, nullptr, nullptr, nullptr,
                        fs_name, static_cast<::DWORD>(
                                     sizeof(fs_name) / sizeof(fs_name[0]))) !=
                    FALSE) {
                    const result<std::string> converted_type =
                        wide_to_utf8(fs_name);
                    if (!converted_type) {
                        return fail(converted_type.error());
                    }
                    filesystem_type = *converted_type;
                } else {
                    const std::error_code error = last_error();
                    if (error != std::error_code(ERROR_UNRECOGNIZED_VOLUME,
                                                 std::system_category()) &&
                        error != std::error_code(ERROR_NOT_READY,
                                                 std::system_category())) {
                        return fail(error);
                    }
                }

                result<std::vector<volume_extent_record>> converted_extents =
                    convert_volume_extents(*extent_buffer, mount_point,
                                           filesystem_type);
                if (!converted_extents) {
                    return fail(converted_extents.error());
                }
                for (volume_extent_record& extent : *converted_extents) {
                    extents_list.push_back(std::move(extent));
                }
            }
        }

        std::fill(volume_buffer.begin(), volume_buffer.end(), L'\0');
        if (::FindNextVolumeW(
                raw_find, volume_buffer.data(),
                static_cast<::DWORD>(volume_buffer.size())) == FALSE) {
            const std::error_code error = last_error();
            if (error == std::error_code(ERROR_NO_MORE_FILES,
                                         std::system_category())) {
                break;
            }
            return fail(error);
        }
    }
    return extents_list;
}

/// Formats a GUID as a standard lowercase hyphenated 36-character string.
inline std::string format_guid(const ::GUID& guid) {
    char buffer[37];
    static const char hex_digits[] = "0123456789abcdef";
    const auto write_hex2 = [](char* dest, unsigned char val) {
        dest[0] = hex_digits[(val >> 4) & 0x0F];
        dest[1] = hex_digits[val & 0x0F];
    };
    const auto write_hex4 = [&write_hex2](char* dest, unsigned short val) {
        write_hex2(dest, static_cast<unsigned char>((val >> 8) & 0xFF));
        write_hex2(dest + 2, static_cast<unsigned char>(val & 0xFF));
    };
    const auto write_hex8 = [&write_hex4](char* dest, unsigned long val) {
        write_hex4(dest, static_cast<unsigned short>((val >> 16) & 0xFFFF));
        write_hex4(dest + 4, static_cast<unsigned short>(val & 0xFFFF));
    };

    write_hex8(buffer, guid.Data1);
    buffer[8] = '-';
    write_hex4(buffer + 9, guid.Data2);
    buffer[13] = '-';
    write_hex4(buffer + 14, guid.Data3);
    buffer[18] = '-';
    write_hex2(buffer + 19, guid.Data4[0]);
    write_hex2(buffer + 21, guid.Data4[1]);
    buffer[23] = '-';
    for (int i = 2; i < 8; ++i) {
        write_hex2(buffer + 24 + (i - 2) * 2, guid.Data4[i]);
    }
    buffer[36] = '\0';
    return std::string(buffer, 36U);
}

/// Formats an MBR partition type byte as a lowercase hex string (e.g. "0x07").
inline std::string format_mbr_type(std::uint8_t type) {
    static const char hex_digits[] = "0123456789abcdef";
    char buffer[5];
    buffer[0] = '0';
    buffer[1] = 'x';
    buffer[2] = hex_digits[(type >> 4) & 0x0F];
    buffer[3] = hex_digits[type & 0x0F];
    buffer[4] = '\0';
    return std::string(buffer, 4U);
}

/// Converts a DRIVE_LAYOUT_INFORMATION_EX buffer into partition records.
inline result<std::vector<storage_common::partition_record>>
convert_drive_layout(const std::vector<char>& buffer,
                     unsigned int drive_index) {
    if (buffer.size() < sizeof(::DRIVE_LAYOUT_INFORMATION_EX)) {
        return fail(errc::malformed_data);
    }
    ::DRIVE_LAYOUT_INFORMATION_EX layout;
    std::memcpy(&layout, buffer.data(), sizeof(layout));

    using storage_common::partition_scheme_classification;
    partition_scheme_classification default_scheme =
        partition_scheme_classification::unknown;
    if (layout.PartitionStyle == PARTITION_STYLE_MBR) {
        default_scheme = partition_scheme_classification::mbr;
    } else if (layout.PartitionStyle == PARTITION_STYLE_GPT) {
        default_scheme = partition_scheme_classification::gpt;
    } else if (layout.PartitionStyle == PARTITION_STYLE_RAW) {
        default_scheme = partition_scheme_classification::raw;
    }

    if (layout.PartitionCount > 0U) {
        const std::size_t available_extra =
            (buffer.size() - sizeof(::DRIVE_LAYOUT_INFORMATION_EX)) /
            sizeof(::PARTITION_INFORMATION_EX);
        if (static_cast<std::size_t>(layout.PartitionCount - 1U) >
            available_extra) {
            return fail(errc::malformed_data);
        }
    }

    const std::size_t base_offset =
        offsetof(::DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry);

    std::vector<storage_common::partition_record> records;
    for (::DWORD i = 0U; i < layout.PartitionCount; ++i) {
        ::PARTITION_INFORMATION_EX entry;
        const std::size_t entry_offset =
            base_offset +
            static_cast<std::size_t>(i) * sizeof(::PARTITION_INFORMATION_EX);
        std::memcpy(&entry, buffer.data() + entry_offset, sizeof(entry));

        if (entry.PartitionLength.QuadPart <= 0 ||
            entry.PartitionNumber == 0U) {
            continue;
        }

        storage_common::partition_record record;
        record.identifier = "PhysicalDrive" + std::to_string(drive_index) +
                            "Partition" + std::to_string(entry.PartitionNumber);
        record.disk_identifier =
            "PhysicalDrive" + std::to_string(drive_index);
        record.partition_number = static_cast<std::uint32_t>(entry.PartitionNumber);
        if (entry.StartingOffset.QuadPart >= 0) {
            record.has_start_offset_bytes = true;
            record.start_offset_bytes =
                static_cast<std::uint64_t>(entry.StartingOffset.QuadPart);
        }
        record.has_size_bytes = true;
        record.size_bytes =
            static_cast<std::uint64_t>(entry.PartitionLength.QuadPart);
        record.scheme = default_scheme;

        if (entry.PartitionStyle == PARTITION_STYLE_MBR) {
            record.scheme = partition_scheme_classification::mbr;
            record.is_bootable = (entry.Mbr.BootIndicator != FALSE);
            record.has_type_identifier = true;
            record.type_identifier =
                format_mbr_type(entry.Mbr.PartitionType);
        } else if (entry.PartitionStyle == PARTITION_STYLE_GPT) {
            record.scheme = partition_scheme_classification::gpt;
            record.has_type_identifier = true;
            record.type_identifier = format_guid(entry.Gpt.PartitionType);
            record.has_uuid = true;
            record.uuid = format_guid(entry.Gpt.PartitionId);
            // Check GPT read-only attribute bit
            if ((entry.Gpt.Attributes & 0x1000000000000000ULL) != 0U) {
                record.is_read_only = true;
            }
            if (entry.Gpt.Name[0] != L'\0') {
                std::size_t name_len = 0U;
                while (name_len < 36U && entry.Gpt.Name[name_len] != L'\0') {
                    ++name_len;
                }
                if (name_len > 0U) {
                    const result<std::string> name_utf8 =
                        wide_to_utf8(std::wstring_view(entry.Gpt.Name, name_len));
                    if (!name_utf8) { return fail(name_utf8.error()); }
                    std::string trimmed = *name_utf8;
                    while (!trimmed.empty() && trimmed.back() == ' ') {
                        trimmed.pop_back();
                    }
                    if (!trimmed.empty()) {
                        record.has_name = true;
                        record.name = std::move(trimmed);
                    }
                }
            }
        }

        records.push_back(std::move(record));
    }
    return records;
}

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

/// Enumerates partitions across all physical drives using the given API.
template <typename Api>
result<std::vector<storage_common::partition_record>> enumerate_partitions(
    const Api& api) {
    std::vector<storage_common::partition_record> records;
    const result<std::vector<unsigned int>> indices = api.drive_indices();
    if (!indices) { return fail(indices.error()); }

    const result<std::vector<volume_extent_record>> extents =
        api.volume_extents();
    if (!extents) { return fail(extents.error()); }

    for (const unsigned int index : *indices) {
        const result<::HANDLE> opened = api.open_drive(index);
        if (!opened) {
            if (is_drive_population_race(opened.error())) { continue; }
            return fail(opened.error());
        }
        const api_device_handle<Api> owned(api, *opened);

        const result<std::vector<char>> layout_buffer =
            api.query_layout(*opened);
        if (!layout_buffer) {
            if (layout_buffer.error() ==
                    std::error_code(ERROR_NOT_SUPPORTED,
                                    std::system_category()) ||
                layout_buffer.error() ==
                    std::error_code(ERROR_INVALID_FUNCTION,
                                    std::system_category())) {
                continue;
            }
            return fail(layout_buffer.error());
        }

        const result<std::vector<storage_common::partition_record>>
            drive_parts = convert_drive_layout(*layout_buffer, index);
        if (!drive_parts) { return fail(drive_parts.error()); }

        for (auto part : *drive_parts) {
            for (const auto& ext : *extents) {
                if (ext.disk_number == index &&
                    part.has_start_offset_bytes &&
                    ext.start_offset_bytes == part.start_offset_bytes) {
                    if (!ext.mount_point.empty()) {
                        part.is_mounted = true;
                        part.mount_point = ext.mount_point;
                    }
                    if (!ext.filesystem_type.empty()) {
                        part.has_filesystem_type = true;
                        part.filesystem_type = ext.filesystem_type;
                    }
                    break;
                }
            }
            records.push_back(std::move(part));
        }
    }
    std::sort(records.begin(), records.end(),
              [](const storage_common::partition_record& left,
                 const storage_common::partition_record& right) {
                  return left.identifier < right.identifier;
              });
    return records;
}

inline result<std::vector<storage_common::drive_record>> drives() {
    return enumerate_drives(native_drive_api{});
}

inline result<std::vector<storage_common::partition_record>> partitions() {
    return enumerate_partitions(native_drive_api{});
}

/// Converts one STORAGE_PREDICT_FAILURE buffer into its boolean failure prediction result.
inline result<bool> convert_predict_failure(const std::vector<char>& buffer) {
    if (buffer.size() < sizeof(::STORAGE_PREDICT_FAILURE)) {
        return fail(errc::malformed_data);
    }
    ::STORAGE_PREDICT_FAILURE predict;
    std::memcpy(&predict, buffer.data(), sizeof(predict));
    return predict.PredictFailure != 0;
}

/// Collects health and failure prediction facts for one PhysicalDrive index using the given API.
template <typename Api>
result<storage_common::health_record> collect_drive_health(
    const Api& api, unsigned int index) {
    const result<::HANDLE> opened = api.open_drive(index);
    if (!opened) {
        return fail(opened.error());
    }
    const api_device_handle<Api> owned(api, *opened);

    storage_common::health_record record;
    record.identifier = "PhysicalDrive" + std::to_string(index);
    record.status = storage_common::health_status_classification::unknown;
    record.has_failure_predicted = false;

    const result<std::vector<char>> predict_buffer =
        api.query_predict_failure(*opened);
    if (predict_buffer) {
        const result<bool> predicted = convert_predict_failure(*predict_buffer);
        if (!predicted) { return fail(predicted.error()); }
        record.has_failure_predicted = true;
        record.failure_predicted = *predicted;
        if (*predicted) {
            record.status =
                storage_common::health_status_classification::warning;
        } else {
            record.status =
                storage_common::health_status_classification::healthy;
        }
    } else {
        const std::error_code err = predict_buffer.error();
        if (err == std::error_code(ERROR_NOT_SUPPORTED, std::system_category()) ||
            err == std::error_code(ERROR_INVALID_FUNCTION, std::system_category()) ||
            err == std::error_code(ERROR_CALL_NOT_IMPLEMENTED, std::system_category())) {
            // Not supported by the device or driver; leave status as unknown.
        } else {
            return fail(err);
        }
    }

    return record;
}

/// Enumerates health records across all physical drives using the given API.
template <typename Api>
result<std::vector<storage_common::health_record>> enumerate_all_drive_health(
    const Api& api) {
    std::vector<storage_common::health_record> records;
    const result<std::vector<unsigned int>> indices = api.drive_indices();
    if (!indices) { return fail(indices.error()); }
    for (const unsigned int index : *indices) {
        const result<storage_common::health_record> h =
            collect_drive_health(api, index);
        if (!h) {
            if (is_drive_population_race(h.error())) { continue; }
            return fail(h.error());
        }
        records.push_back(*h);
    }
    std::sort(records.begin(), records.end(),
              [](const storage_common::health_record& left,
                 const storage_common::health_record& right) {
                  return left.identifier < right.identifier;
              });
    return records;
}

inline result<storage_common::health_record> health(
    std::string_view disk_identifier) {
    if (!is_valid_utf8(disk_identifier)) {
        return fail(errc::invalid_encoding);
    }
    if (!storage_common::is_valid_disk_identifier(disk_identifier)) {
        return fail(errc::invalid_argument);
    }

    std::string_view num_str = disk_identifier;
    std::string_view prefix = "PhysicalDrive";
    if (num_str.rfind(prefix, 0) == 0) {
        num_str.remove_prefix(prefix.size());
    }
    if (num_str.empty()) { return fail(errc::invalid_argument); }

    unsigned int index = 0U;
    constexpr unsigned int max_val =
        (std::numeric_limits<unsigned int>::max)();
    for (char c : num_str) {
        if (c < '0' || c > '9') { return fail(errc::invalid_argument); }
        const unsigned int digit = static_cast<unsigned int>(c - '0');
        if (index > (max_val - digit) / 10U) {
            return fail(errc::value_too_large);
        }
        index = index * 10U + digit;
    }
    return collect_drive_health(native_drive_api{}, index);
}

inline result<std::vector<storage_common::health_record>> all_drive_health() {
    return enumerate_all_drive_health(native_drive_api{});
}

} // namespace storage_backend
} // namespace detail
} // namespace syscape

#endif
