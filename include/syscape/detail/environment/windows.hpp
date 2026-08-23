#ifndef SYSCAPE_DETAIL_ENVIRONMENT_WINDOWS_HPP
#define SYSCAPE_DETAIL_ENVIRONMENT_WINDOWS_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <io.h>
#include <shlobj.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <syscape/detail/environment/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace environment_backend {

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

inline result<std::wstring> utf8_to_wide(std::string_view value) {
    static_assert(sizeof(wchar_t) == sizeof(char16_t),
                  "The Windows backend requires 16-bit wchar_t");
    if (value.empty()) {
        return std::wstring();
    }
    const result<std::u16string> converted = detail::utf8_to_utf16(value);
    if (!converted) {
        return fail(converted.error());
    }
    std::wstring wide;
    wide.reserve(converted->size());
    for (char16_t unit : *converted) {
        wide.push_back(static_cast<wchar_t>(unit));
    }
    return wide;
}

inline std::error_code last_error() noexcept {
    const ::DWORD err = ::GetLastError();
    return std::error_code(static_cast<int>(err), std::system_category());
}

/// Error category for HRESULT values outside the Win32 facility.
class hresult_error_category final : public std::error_category {
public:
    const char* name() const noexcept override {
        return "windows-hresult";
    }

    std::string message(int value) const override {
        static const char digits[] = "0123456789ABCDEF";
        const unsigned int raw = static_cast<unsigned int>(value);
        std::string text("Windows HRESULT 0x");
        for (int shift = 28; shift >= 0; shift -= 4) {
            text.push_back(digits[(raw >> shift) & 0xFU]);
        }
        return text;
    }
};

inline const std::error_category& hresult_category() noexcept {
    static const hresult_error_category category;
    return category;
}

inline std::error_code map_hresult(::HRESULT value) noexcept {
    if (HRESULT_FACILITY(value) == FACILITY_WIN32) {
        return std::error_code(static_cast<int>(HRESULT_CODE(value)),
                               std::system_category());
    }
    return std::error_code(static_cast<int>(value), hresult_category());
}

class co_task_memory {
public:
    explicit co_task_memory(::PWSTR value) noexcept : value_(value) {}
    co_task_memory(const co_task_memory&) = delete;
    co_task_memory& operator=(const co_task_memory&) = delete;
    ~co_task_memory() {
        if (value_ != nullptr) {
            ::CoTaskMemFree(value_);
        }
    }

    ::PWSTR get() const noexcept { return value_; }

private:
    ::PWSTR value_;
};

inline result<std::string> get(std::string_view name) {
    const result<void> check = environment_common::validate_variable_name(name);
    if (!check) {
        return fail(check.error());
    }

    const result<std::wstring> wide_name = utf8_to_wide(name);
    if (!wide_name) {
        return fail(wide_name.error());
    }

    // Attempt retrieval with retry loop for size races
    constexpr std::size_t initial_buffer_size = 256U;
    constexpr std::size_t maximum_env_size = 32767U; // Win32 environment limit
    std::vector<wchar_t> buffer(initial_buffer_size);

    for (std::size_t attempt = 0; attempt < 5; ++attempt) {
        ::SetLastError(ERROR_SUCCESS);
        const ::DWORD copied = ::GetEnvironmentVariableW(
            wide_name->c_str(), buffer.data(), static_cast<::DWORD>(buffer.size()));

        if (copied == 0U) {
            const ::DWORD err = ::GetLastError();
            if (err == ERROR_ENVVAR_NOT_FOUND) {
                return fail(errc::not_found);
            }
            if (err == ERROR_SUCCESS) {
                // The environment variable exists and its value is an empty string.
                return std::string();
            }
            return fail(std::error_code(static_cast<int>(err), std::system_category()));
        }

        if (copied < buffer.size()) {
            return wide_to_utf8(std::wstring_view(buffer.data(), copied));
        }

        // Buffer was too small; copied is the required size including null terminator.
        if (copied > maximum_env_size) {
            return fail(errc::malformed_data);
        }
        buffer.resize(static_cast<std::size_t>(copied));
    }

    return fail(errc::temporarily_unavailable);
}

inline result<bool> has(std::string_view name) {
    const result<void> check = environment_common::validate_variable_name(name);
    if (!check) {
        return fail(check.error());
    }

    const result<std::wstring> wide_name = utf8_to_wide(name);
    if (!wide_name) {
        return fail(wide_name.error());
    }

    ::SetLastError(ERROR_SUCCESS);
    const ::DWORD size = ::GetEnvironmentVariableW(wide_name->c_str(), nullptr, 0U);
    if (size == 0U) {
        const ::DWORD err = ::GetLastError();
        if (err == ERROR_ENVVAR_NOT_FOUND) {
            return false;
        }
        if (err == ERROR_SUCCESS) {
            // Variable exists and is empty
            return true;
        }
        return fail(std::error_code(static_cast<int>(err), std::system_category()));
    }
    return true;
}

inline result<std::string> get_known_folder(REFKNOWNFOLDERID folder_id) {
    ::PWSTR raw = nullptr;
    const ::HRESULT outcome =
        ::SHGetKnownFolderPath(folder_id, 0U, nullptr, &raw);
    const co_task_memory guard(raw);
    if (FAILED(outcome)) {
        return fail(map_hresult(outcome));
    }

    const std::wstring value(guard.get() != nullptr ? guard.get() : L"");
    if (value.empty()) {
        return fail(errc::malformed_data);
    }

    const result<std::string> converted = wide_to_utf8(value);
    if (!converted) {
        return converted;
    }
    if (converted->empty()) {
        return fail(errc::malformed_data);
    }
    return environment_common::normalize_directory_path(*converted);
}

inline result<std::string> home_directory() {
    return get_known_folder(::FOLDERID_Profile);
}

inline result<std::string> temp_directory() {
    wchar_t buffer[MAX_PATH + 2];
    const ::DWORD len = ::GetTempPathW(MAX_PATH + 2, buffer);
    if (len == 0U) {
        return fail(last_error());
    }
    if (len > MAX_PATH + 2U) {
        return fail(errc::temporarily_unavailable);
    }
    const result<std::string> converted = wide_to_utf8(std::wstring_view(buffer, len));
    if (!converted) {
        return converted;
    }
    return environment_common::normalize_directory_path(*converted);
}

inline result<std::string> config_directory() {
    return get_known_folder(::FOLDERID_RoamingAppData);
}

inline result<std::string> data_directory() {
    return get_known_folder(::FOLDERID_LocalAppData);
}

inline result<std::string> cache_directory() {
    return get_known_folder(::FOLDERID_LocalAppData);
}

inline result<bool> is_interactive_handle(int crt_fd, ::DWORD std_handle_id) {
    if (::_isatty(crt_fd) == 0) {
        return false;
    }
    const ::HANDLE handle = ::GetStdHandle(std_handle_id);
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr) {
        return false;
    }
    ::DWORD mode = 0;
    if (::GetConsoleMode(handle, &mode) == 0) {
        return false;
    }
    return true;
}

inline result<bool> is_interactive_stdin() {
    return is_interactive_handle(0, STD_INPUT_HANDLE);
}

inline result<bool> is_interactive_stdout() {
    return is_interactive_handle(1, STD_OUTPUT_HANDLE);
}

inline result<bool> is_interactive_stderr() {
    return is_interactive_handle(2, STD_ERROR_HANDLE);
}

} // namespace environment_backend
} // namespace detail
} // namespace syscape

#endif
