#ifndef SYSCAPE_DETAIL_FILESYSTEM_WINDOWS_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_WINDOWS_HPP

#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <windows.h>

#include <syscape/detail/filesystem/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace filesystem_backend {

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

/// Converts UTF-16 text to a wide string code unit by code unit.
///
/// The types are distinct even where their width matches, so a pointer
/// reinterpretation would violate strict aliasing; element-wise conversion
/// keeps the strict standard-C++ contract.
inline std::wstring utf16_to_wide(const std::u16string& value) {
    static_assert(sizeof(wchar_t) == sizeof(char16_t),
                  "The Windows backend requires 16-bit wchar_t");
    std::wstring wide;
    wide.reserve(value.size());
    for (char16_t unit : value) {
        wide.push_back(static_cast<wchar_t>(unit));
    }
    return wide;
}

inline std::error_code last_error() noexcept {
    return std::error_code(static_cast<int>(::GetLastError()),
                           std::system_category());
}

/// Platform calls used to enumerate drive-letter volumes.
///
/// The indirection exists so tests can drive enumeration with synthetic
/// data instead of real drives; production callers always use the native
/// implementation.
struct native_drive_api {
    /// Returns the drive-letter bitmask from GetLogicalDrives.
    static result<::DWORD> logical_drives() {
        const ::DWORD mask = ::GetLogicalDrives();
        if (mask == 0U) { return fail(last_error()); }
        return mask;
    }

    /// Returns the native device name behind a drive letter such as "X:".
    ///
    /// The mapping is best-effort: a letter whose DOS device name cannot be
    /// queried still names a real mount point, so the failure degrades to an
    /// empty source instead of discarding the entry.
    static result<std::wstring> dos_device(const std::wstring& letter) {
        wchar_t buffer[1024];
        const ::DWORD stored =
            ::QueryDosDeviceW(letter.c_str(), buffer,
                              static_cast<::DWORD>(
                                  sizeof(buffer) / sizeof(buffer[0])));
        if (stored == 0U) { return std::wstring(); }
        const std::size_t length =
            ::wcsnlen(buffer, sizeof(buffer) / sizeof(buffer[0]));
        return std::wstring(buffer, length);
    }

    /// Returns the file-system name reported for a volume root such as
    /// "X:\". A letter without ready media, a locked volume, or an empty
    /// reported name yields a failure so the enumerator can omit the
    /// letter instead of inventing a type.
    static result<std::wstring> file_system_name(const std::wstring& root) {
        wchar_t name[256];
        if (::GetVolumeInformationW(root.c_str(), nullptr, 0U, nullptr,
                                    nullptr, nullptr, name,
                                    static_cast<::DWORD>(
                                        sizeof(name) / sizeof(name[0]))) ==
            FALSE) {
            return fail(last_error());
        }
        const std::size_t length =
            ::wcsnlen(name, sizeof(name) / sizeof(name[0]));
        return std::wstring(name, length);
    }
};

/// Builds mount records from a drive-letter bitmask through the given API.
///
/// Drive letters whose file-system information cannot be queried are
/// omitted; they name no mounted volume at the moment of the query. This
/// slice enumerates drive letters only, so network shares and mounted
/// folders without letters are outside its documented scope.
template <typename Api>
result<std::vector<filesystem_common::mount_record>> enumerate_drive_mounts(
    const Api& api) {
    const result<::DWORD> mask = api.logical_drives();
    if (!mask) { return fail(mask.error()); }

    std::vector<filesystem_common::mount_record> records;
    for (unsigned int bit = 0; bit < 26U; ++bit) {
        constexpr ::DWORD first_letter = 1U;
        const ::DWORD letter_mask =
            static_cast<::DWORD>(first_letter << bit);
        if ((*mask & letter_mask) == 0U) { continue; }

        std::wstring root;
        root.reserve(3U);
        root.push_back(static_cast<wchar_t>(L'A' + bit));
        root.push_back(L':');
        root.push_back(L'\\');
        const std::wstring letter(root, 0U, 2U);

        filesystem_common::mount_record record;
        const result<std::wstring> device = api.dos_device(letter);
        if (!device) { return fail(device.error()); }
        result<std::string> source = wide_to_utf8(*device);
        if (!source) { return fail(source.error()); }
        record.source = std::move(*source);

        const result<std::wstring> type = api.file_system_name(root);
        if (!type || type->empty()) { continue; }
        result<std::string> converted_type = wide_to_utf8(*type);
        if (!converted_type) { return fail(converted_type.error()); }
        record.file_system_type = std::move(*converted_type);

        result<std::string> mount_point = wide_to_utf8(root);
        if (!mount_point) { return fail(mount_point.error()); }
        record.mount_point = std::move(*mount_point);
        records.push_back(std::move(record));
    }
    return records;
}

inline result<std::vector<filesystem_common::mount_record>> mounts() {
    return enumerate_drive_mounts(native_drive_api{});
}

/// Returns the mount point of the volume holding the given path through
/// the documented GetVolumePathNameW interface.
///
/// The interface accepts file and directory names, absolute and relative
/// paths, and resolves junction points and mounted folders to the volume
/// actually holding the path endpoint. Two documented quirks are handled
/// explicitly: a buffer exactly one character short succeeds but omits the
/// trailing backslash, which would turn a drive root into a
/// drive-relative name, so the result is normalized to end with one; an
/// empty input fails with ERROR_SUCCESS, so the caller rejects empty paths
/// beforehand and a zero error code here is reported as malformed data.
inline result<std::wstring> volume_mount_point(const std::wstring& file) {
    constexpr ::DWORD maximum_characters = 32768U;
    if (file.empty()) { return fail(errc::invalid_argument); }
    ::DWORD capacity = 260U;
    for (;;) {
        std::wstring buffer(capacity, L'\0');
        if (::GetVolumePathNameW(file.c_str(), &buffer[0], capacity) !=
            FALSE) {
            const std::size_t length =
                ::wcsnlen(buffer.c_str(), capacity);
            if (length == 0U) { return fail(errc::malformed_data); }
            std::wstring mount_point(buffer, 0U, length);
            if (mount_point.back() != L'\\') {
                mount_point.push_back(L'\\');
            }
            return mount_point;
        }
        // The documented interface reports buffer exhaustion as a plain
        // failure without a dedicated error contract, so every failure is
        // retried once into the maximum documented path size before its
        // native error code is treated as authoritative. The empty-input
        // quirk fails with ERROR_SUCCESS, which is not a usable error and
        // is surfaced as malformed platform data instead.
        const ::DWORD error = ::GetLastError();
        if (capacity >= maximum_characters) {
            return error == ERROR_SUCCESS
                       ? result<std::wstring>(fail(errc::malformed_data))
                       : result<std::wstring>(fail(std::error_code(
                             static_cast<int>(error),
                             std::system_category())));
        }
        capacity = capacity >= maximum_characters / 2U
                       ? maximum_characters : capacity * 2U;
    }
}

/// Queries capacity for the volume containing the given path.
///
/// Path input is validated at the public boundary before backend selection.
/// Relative input is first resolved against the current working directory,
/// and the resulting file or directory is checked for existence because
/// GetVolumePathNameW can otherwise succeed for a missing endpoint on an
/// existing volume. The interface then resolves junction points and mounted
/// folders to the mount point of the volume actually holding the endpoint,
/// which serves every capacity query below. Windows exposes no fundamental
/// block size, so the cluster size reported by GetDiskFreeSpaceW is the
/// closest documented equivalent and the public contract documents that
/// approximation.
inline result<filesystem_common::space_snapshot> space(
    const std::string& path) {
    if (path.empty()) { return fail(errc::invalid_argument); }

    const result<std::u16string> converted = utf8_to_utf16(path);
    if (!converted) { return fail(converted.error()); }

    const std::filesystem::path requested(utf16_to_wide(*converted));
    std::error_code resolution;
    const std::filesystem::path absolute =
        std::filesystem::absolute(requested, resolution);
    if (resolution) { return fail(resolution); }

    if (::GetFileAttributesW(absolute.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return fail(last_error());
    }

    const result<std::wstring> volume =
        volume_mount_point(absolute.native());
    if (!volume) { return fail(volume.error()); }

    ::ULARGE_INTEGER available {};
    ::ULARGE_INTEGER total {};
    ::ULARGE_INTEGER free_bytes {};
    if (::GetDiskFreeSpaceExW(volume->c_str(), &available, &total,
                              &free_bytes) == FALSE) {
        return fail(last_error());
    }

    ::DWORD flags = 0U;
    if (::GetVolumeInformationW(volume->c_str(), nullptr, 0U, nullptr,
                                nullptr, &flags, nullptr, 0U) == FALSE) {
        return fail(last_error());
    }

    ::DWORD sectors_per_cluster = 0U;
    ::DWORD bytes_per_sector = 0U;
    if (::GetDiskFreeSpaceW(volume->c_str(), &sectors_per_cluster,
                            &bytes_per_sector, nullptr,
                            nullptr) == FALSE) {
        return fail(last_error());
    }

    filesystem_common::space_snapshot snapshot;
    snapshot.capacity_bytes = static_cast<std::uint64_t>(total.QuadPart);
    snapshot.free_bytes = static_cast<std::uint64_t>(free_bytes.QuadPart);
    snapshot.available_bytes =
        static_cast<std::uint64_t>(available.QuadPart);
    snapshot.block_size_bytes =
        static_cast<std::uint64_t>(sectors_per_cluster) *
        static_cast<std::uint64_t>(bytes_per_sector);
    snapshot.read_only = (flags & FILE_READ_ONLY_VOLUME) != 0U;
    return snapshot;
}

} // namespace filesystem_backend
} // namespace detail
} // namespace syscape

#endif
