#ifndef SYSCAPE_DETAIL_PROCESS_WINDOWS_HPP
#define SYSCAPE_DETAIL_PROCESS_WINDOWS_HPP

#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <syscape/detail/utf8.hpp>
#include <syscape/detail/process/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

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

class snapshot_handle {
public:
    explicit snapshot_handle(::HANDLE value) noexcept : value_(value) {}
    snapshot_handle(const snapshot_handle&) = delete;
    snapshot_handle& operator=(const snapshot_handle&) = delete;
    ~snapshot_handle() {
        if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) {
            static_cast<void>(::CloseHandle(value_));
        }
    }

    ::HANDLE get() const noexcept { return value_; }

private:
    ::HANDLE value_;
};

class local_arguments {
public:
    explicit local_arguments(::LPWSTR* value) noexcept : value_(value) {}
    local_arguments(const local_arguments&) = delete;
    local_arguments& operator=(const local_arguments&) = delete;
    ~local_arguments() {
        if (value_ != nullptr) { static_cast<void>(::LocalFree(value_)); }
    }

    ::LPWSTR* get() const noexcept { return value_; }

private:
    ::LPWSTR* value_;
};

inline std::error_code last_error() noexcept {
    return std::error_code(static_cast<int>(::GetLastError()),
                           std::system_category());
}

inline result<std::uint32_t> process_id() {
    const ::DWORD value = ::GetCurrentProcessId();
    return static_cast<std::uint32_t>(value);
}

inline result<std::uint32_t> parent_process_id() {
    const ::DWORD current_process = ::GetCurrentProcessId();

    const ::HANDLE raw_snapshot =
        ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0U);
    if (raw_snapshot == INVALID_HANDLE_VALUE) { return fail(last_error()); }
    const snapshot_handle snapshot(raw_snapshot);

    ::PROCESSENTRY32W entry {};
    entry.dwSize = sizeof(entry);
    ::BOOL has_entry = ::Process32FirstW(snapshot.get(), &entry);
    if (has_entry == FALSE) {
        const ::DWORD error = ::GetLastError();
        return error == ERROR_NO_MORE_FILES
            ? result<std::uint32_t>(fail(errc::not_found))
            : result<std::uint32_t>(fail(std::error_code(
                  static_cast<int>(error), std::system_category())));
    }

    while (has_entry == TRUE) {
        if (entry.th32ProcessID == current_process) {
            return static_cast<std::uint32_t>(entry.th32ParentProcessID);
        }

        entry.dwSize = sizeof(entry);
        has_entry = ::Process32NextW(snapshot.get(), &entry);
    }

    const ::DWORD error = ::GetLastError();
    return error == ERROR_NO_MORE_FILES ? result<std::uint32_t>(
                                             fail(errc::not_found))
                                        : result<std::uint32_t>(
                                              fail(std::error_code(
                                                  static_cast<int>(error),
                                                  std::system_category())));
}

template <typename QueryOperation>
inline result<std::wstring> grow_wide_string(QueryOperation query,
                                             std::size_t initial_size) {
    constexpr std::size_t maximum_size = 32U * 1024U;
    std::size_t size = initial_size < 1U ? 1U : initial_size;
    std::wstring value(size, L'\0');
    for (;;) {
        const ::DWORD count =
            query(value.data(), static_cast<::DWORD>(value.size()));
        if (count == 0U) {
            if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                return fail(last_error());
            }
            if (size >= maximum_size) {
                return fail(errc::value_too_large);
            }
        } else if (static_cast<std::size_t>(count) <= value.size()) {
            value.resize(count);
            return value;
        } else if (value.size() >= maximum_size) {
            return fail(errc::value_too_large);
        }
        size = value.size() >= maximum_size / 2U ? maximum_size
                                                  : value.size() * 2U;
        value.resize(size);
    }
}

inline result<std::string> executable_path() {
    constexpr std::size_t maximum_size = 32U * 1024U;
    std::vector<wchar_t> native(MAX_PATH);
    for (;;) {
        const ::DWORD count =
            ::GetModuleFileNameW(nullptr, native.data(),
                                 static_cast<::DWORD>(native.size()));
        if (count == 0U) {
            const ::DWORD error = ::GetLastError();
            if (error == ERROR_INSUFFICIENT_BUFFER) {
                if (native.size() >= maximum_size) {
                    return fail(errc::value_too_large);
                }
            } else {
                return fail(last_error());
            }
        } else if (static_cast<std::size_t>(count) < native.size()) {
            const std::wstring path(native.data(), count);
            if (!std::filesystem::path(path).is_absolute()) {
                return fail(errc::malformed_data);
            }
            return wide_to_utf8(path);
        } else if (native.size() >= maximum_size) {
            return fail(errc::value_too_large);
        }

        const std::size_t next_size =
            native.size() >= maximum_size / 2U ? maximum_size
                                               : native.size() * 2U;
        native.resize(next_size);
    }
}

inline result<std::vector<std::string>> command_line() {
    int argument_count = 0;
    ::LPWSTR* arguments = ::CommandLineToArgvW(::GetCommandLineW(),
                                               &argument_count);
    if (arguments == nullptr) { return fail(last_error()); }
    const local_arguments owned_arguments(arguments);

    if (argument_count < 0) { return fail(errc::malformed_data); }
    std::vector<std::string> values;
    if (static_cast<std::size_t>(argument_count) > values.max_size()) {
        return fail(errc::malformed_data);
    }
    values.resize(static_cast<std::size_t>(argument_count));
    for (int index = 0; index < argument_count; ++index) {
        result<std::string> value = wide_to_utf8(arguments[index]);
        if (!value) { return fail(value.error()); }
        values[static_cast<std::size_t>(index)] = std::move(*value);
    }
    return values;
}

inline result<std::string> working_directory() {
    result<std::wstring> native = grow_wide_string(
        [](::LPWSTR buffer, ::DWORD size) {
            return ::GetCurrentDirectoryW(size, buffer);
        },
        MAX_PATH);
    if (!native) { return fail(native.error()); }
    if (native->empty() || !std::filesystem::path(*native).is_absolute()) {
        return fail(errc::malformed_data);
    }
    return wide_to_utf8(*native);
}

/// Converts a hundred-nanosecond amount reported by the platform to a
/// duration.
inline result<std::chrono::nanoseconds> hundred_nanosecond_units_to_duration(
    std::uint64_t units) {
    constexpr std::uint64_t maximum_units = static_cast<std::uint64_t>(
        (std::chrono::nanoseconds::max)().count() / 100U);
    if (units > maximum_units) { return fail(errc::value_too_large); }
    return std::chrono::nanoseconds(units * 100U);
}

/// Reads one FILETIME as its unsigned hundred-nanosecond count.
inline std::uint64_t filetime_units(const ::FILETIME& value) noexcept {
    return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32U) |
           static_cast<std::uint64_t>(value.dwLowDateTime);
}

/// Converts a wall-clock FILETIME value to a system-clock time point.
///
/// FILETIME counts hundred-nanosecond intervals since 1601-01-01 UTC. Values
/// before the 1970 Unix epoch cannot be represented and are malformed
/// platform data.
inline result<std::chrono::system_clock::time_point> filetime_to_time_point(
    const ::FILETIME& value) {
    using clock = std::chrono::system_clock;
    // 11644473600 seconds separate the 1601 FILETIME and 1970 Unix epochs.
    constexpr std::uint64_t unix_epoch_offset_units = 116444736000000000ULL;
    const std::uint64_t raw_units = filetime_units(value);
    if (raw_units < unix_epoch_offset_units) {
        return fail(errc::malformed_data);
    }
    const result<std::chrono::nanoseconds> since_unix_epoch =
        hundred_nanosecond_units_to_duration(raw_units -
                                             unix_epoch_offset_units);
    if (!since_unix_epoch) { return fail(since_unix_epoch.error()); }
    return clock::time_point(
        std::chrono::duration_cast<clock::duration>(*since_unix_epoch));
}

inline result<process_common::cpu_time_usage> cpu_time() {
    ::FILETIME creation {};
    ::FILETIME exit_time {};
    ::FILETIME user {};
    ::FILETIME system {};
    if (::GetProcessTimes(::GetCurrentProcess(), &creation, &exit_time,
                          &user, &system) == 0) {
        return fail(last_error());
    }
    const result<std::chrono::nanoseconds> user_duration =
        hundred_nanosecond_units_to_duration(filetime_units(user));
    if (!user_duration) { return fail(user_duration.error()); }
    const result<std::chrono::nanoseconds> system_duration =
        hundred_nanosecond_units_to_duration(filetime_units(system));
    if (!system_duration) { return fail(system_duration.error()); }
    process_common::cpu_time_usage usage;
    usage.user = *user_duration;
    usage.system = *system_duration;
    return usage;
}

inline result<std::chrono::system_clock::time_point> start_time() {
    ::FILETIME creation {};
    ::FILETIME exit_time {};
    ::FILETIME user {};
    ::FILETIME system {};
    if (::GetProcessTimes(::GetCurrentProcess(), &creation, &exit_time,
                          &user, &system) == 0) {
        return fail(last_error());
    }
    return filetime_to_time_point(creation);
}

/// One simplified address-space region description used by the walk seam.
struct region_description {
    /// True when the region is reserved or committed rather than free.
    bool reserved_or_committed = false;
    /// Region extent in bytes. Never zero for a real walk element.
    std::uint64_t size_bytes = 0U;
};

/// Sums the extents of non-free regions between the application minimum and
/// maximum addresses.
///
/// The query seam keeps the documented VirtualQuery walk testable with
/// synthetic region chains, including zero-sized and overflow cases.
template <typename QueryRegion>
inline result<std::uint64_t> sum_address_space(QueryRegion query_region,
                                               std::uintptr_t minimum_address,
                                               std::uintptr_t maximum_address) {
    std::uint64_t total = 0U;
    std::uintptr_t address = minimum_address;
    while (address <= maximum_address) {
        const result<region_description> region = query_region(address);
        if (!region) { return fail(region.error()); }
        if (region->size_bytes == 0U) { return fail(errc::malformed_data); }
        if (region->reserved_or_committed) {
            if (total >
                (std::numeric_limits<std::uint64_t>::max)() -
                    region->size_bytes) {
                return fail(errc::value_too_large);
            }
            total += region->size_bytes;
        }
        if (address > maximum_address - region->size_bytes) { break; }
        address += region->size_bytes;
    }
    return total;
}

inline result<process_common::memory_usage_snapshot> memory_usage() {
    ::PROCESS_MEMORY_COUNTERS counters {};
    counters.cb = static_cast<::DWORD>(sizeof(counters));
    if (::GetProcessMemoryInfo(::GetCurrentProcess(), &counters,
                               static_cast<::DWORD>(sizeof(counters))) == 0) {
        return fail(last_error());
    }

    ::SYSTEM_INFO information {};
    ::GetSystemInfo(&information);
    const auto minimum_address = reinterpret_cast<std::uintptr_t>(
        information.lpMinimumApplicationAddress);
    const auto maximum_address = reinterpret_cast<std::uintptr_t>(
        information.lpMaximumApplicationAddress);
    const result<std::uint64_t> virtual_bytes = sum_address_space(
        [](std::uintptr_t address) -> result<region_description> {
            ::MEMORY_BASIC_INFORMATION region {};
            if (::VirtualQuery(reinterpret_cast<::LPCVOID>(address), &region,
                               static_cast<::DWORD>(sizeof(region))) == 0) {
                return fail(last_error());
            }
            region_description described;
            described.reserved_or_committed = region.State != MEM_FREE;
            described.size_bytes =
                static_cast<std::uint64_t>(region.RegionSize);
            return described;
        },
        minimum_address, maximum_address);
    if (!virtual_bytes) { return fail(virtual_bytes.error()); }

    process_common::memory_usage_snapshot usage;
    usage.resident_bytes = static_cast<std::uint64_t>(counters.WorkingSetSize);
    usage.virtual_bytes = *virtual_bytes;
    return usage;
}

inline result<std::uint32_t> thread_count() {
    const ::DWORD current_process = ::GetCurrentProcessId();

    const ::HANDLE raw_snapshot =
        ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0U);
    if (raw_snapshot == INVALID_HANDLE_VALUE) { return fail(last_error()); }
    const snapshot_handle snapshot(raw_snapshot);

    ::THREADENTRY32 entry {};
    entry.dwSize = sizeof(entry);
    ::BOOL has_entry = ::Thread32First(snapshot.get(), &entry);
    if (has_entry == FALSE) {
        const ::DWORD error = ::GetLastError();
        return error == ERROR_NO_MORE_FILES
            ? result<std::uint32_t>(fail(errc::not_found))
            : result<std::uint32_t>(fail(std::error_code(
                  static_cast<int>(error), std::system_category())));
    }

    std::uint32_t threads = 0U;
    while (has_entry == TRUE) {
        if (entry.th32OwnerProcessID == current_process) {
            if (threads == (std::numeric_limits<std::uint32_t>::max)()) {
                return fail(errc::value_too_large);
            }
            ++threads;
        }
        entry.dwSize = sizeof(entry);
        has_entry = ::Thread32Next(snapshot.get(), &entry);
    }

    const ::DWORD error = ::GetLastError();
    if (error != ERROR_NO_MORE_FILES) {
        return fail(std::error_code(static_cast<int>(error),
                                    std::system_category()));
    }
    // A live process owns at least the calling thread; an empty snapshot
    // cannot describe the current process and is malformed platform data.
    if (threads == 0U) { return fail(errc::not_found); }
    return threads;
}

} // namespace process_backend
} // namespace detail
} // namespace syscape

#endif
