#ifndef SYSCAPE_DETAIL_PROCESS_WINDOWS_HPP
#define SYSCAPE_DETAIL_PROCESS_WINDOWS_HPP

#include <climits>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <syscape/detail/utf8.hpp>
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

} // namespace process_backend
} // namespace detail
} // namespace syscape

#endif
