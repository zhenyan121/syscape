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

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
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
    std::wstring result;
    result.reserve(converted->size());
    for (char16_t unit : *converted) {
        result.push_back(static_cast<wchar_t>(unit));
    }
    return result;
}

inline std::error_code last_error() noexcept {
    const ::DWORD err = ::GetLastError();
    return std::error_code(
        static_cast<int>(err != ERROR_SUCCESS ? err : ERROR_GEN_FAILURE),
        std::system_category());
}

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

inline result<std::string> get_raw(std::string_view name) {
    const result<std::wstring> wide_name = utf8_to_wide(name);
    if (!wide_name) {
        return fail(wide_name.error());
    }

    constexpr std::size_t initial_buffer_size = 256;
    std::vector<wchar_t> buffer(initial_buffer_size);

    for (std::size_t attempt = 0; attempt < 3; ++attempt) {
        ::SetLastError(ERROR_SUCCESS);
        const ::DWORD copied = ::GetEnvironmentVariableW(
            wide_name->c_str(), buffer.data(),
            static_cast<::DWORD>(buffer.size()));

        if (copied == 0U) {
            const ::DWORD err = ::GetLastError();
            if (err == ERROR_ENVVAR_NOT_FOUND) {
                return fail(errc::not_found);
            }
            return fail(std::error_code(
                static_cast<int>(err != ERROR_SUCCESS ? err : ERROR_GEN_FAILURE),
                std::system_category()));
        }

        if (copied < buffer.size()) {
            return wide_to_utf8(std::wstring_view(buffer.data(), copied));
        }

        buffer.resize(static_cast<std::size_t>(copied));
    }

    return fail(errc::temporarily_unavailable);
}

inline result<std::string> get(std::string_view name) {
    const result<void> check = environment_common::validate_variable_name(name);
    if (!check) {
        return fail(check.error());
    }
    return get_raw(name);
}

inline result<bool> has(std::string_view name) {
    const result<void> check = environment_common::validate_variable_name(name);
    if (!check) {
        return fail(check.error());
    }
    const result<std::string> value = get_raw(name);
    if (value) {
        return true;
    }
    if (value.error() == errc::not_found) {
        return false;
    }
    return fail(value.error());
}

inline result<std::vector<::syscape::environment::environment_variable>>
environment_variables() {
    ::LPWCH env_block = ::GetEnvironmentStringsW();
    if (env_block == nullptr) {
        return fail(last_error());
    }

    struct env_strings_guard {
        ::LPWCH ptr;
        ~env_strings_guard() {
            if (ptr != nullptr) {
                ::FreeEnvironmentStringsW(ptr);
            }
        }
    } guard{env_block};

    std::vector<::syscape::environment::environment_variable> vars;
    const wchar_t* current = env_block;
    while (*current != L'\0') {
        const std::wstring_view entry(current);
        current += entry.size() + 1U;

        // Skip hidden CMD variables starting with '=' (e.g. "=C:")
        if (entry.empty() || entry.front() == L'=') {
            continue;
        }

        const std::size_t eq_pos = entry.find(L'=');
        if (eq_pos == std::wstring_view::npos || eq_pos == 0U) {
            continue;
        }

        const std::wstring_view wide_name = entry.substr(0U, eq_pos);
        const std::wstring_view wide_value = entry.substr(eq_pos + 1U);

        const result<std::string> name_utf8 = wide_to_utf8(wide_name);
        if (!name_utf8) {
            return fail(name_utf8.error());
        }
        const result<std::string> value_utf8 = wide_to_utf8(wide_value);
        if (!value_utf8) {
            return fail(value_utf8.error());
        }

        vars.push_back(::syscape::environment::environment_variable{
            std::move(*name_utf8), std::move(*value_utf8)});
    }

    std::sort(vars.begin(), vars.end(),
              [](const ::syscape::environment::environment_variable& a,
                 const ::syscape::environment::environment_variable& b) noexcept {
                  if (a.name != b.name) {
                      return a.name < b.name;
                  }
                  return a.value < b.value;
              });

    return vars;
}

inline result<std::string> current_working_directory() {
    constexpr std::size_t initial_buffer_size = MAX_PATH;
    std::vector<wchar_t> buffer(initial_buffer_size);

    for (std::size_t attempt = 0; attempt < 3; ++attempt) {
        ::SetLastError(ERROR_SUCCESS);
        const ::DWORD copied =
            ::GetCurrentDirectoryW(static_cast<::DWORD>(buffer.size()), buffer.data());

        if (copied == 0U) {
            return fail(last_error());
        }

        if (copied < buffer.size()) {
            const result<std::string> converted =
                wide_to_utf8(std::wstring_view(buffer.data(), copied));
            if (!converted) {
                return fail(converted.error());
            }
            if (converted->empty()) {
                return fail(errc::malformed_data);
            }
            return environment_common::normalize_directory_path(*converted);
        }

        buffer.resize(static_cast<std::size_t>(copied));
    }

    return fail(errc::temporarily_unavailable);
}

inline bool is_valid_executable_file(const wchar_t* path) noexcept {
    const ::DWORD attrs = ::GetFileAttributesW(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0U;
}

inline result<std::string> make_windows_absolute_path(const wchar_t* path) {
    constexpr std::size_t initial_size = MAX_PATH;
    std::vector<wchar_t> full_path(initial_size);

    for (std::size_t attempt = 0; attempt < 3; ++attempt) {
        ::SetLastError(ERROR_SUCCESS);
        const ::DWORD copied = ::GetFullPathNameW(
            path, static_cast<::DWORD>(full_path.size()), full_path.data(), nullptr);

        if (copied == 0U) {
            return fail(last_error());
        }

        if (copied < full_path.size()) {
            const result<std::string> converted =
                wide_to_utf8(std::wstring_view(full_path.data(), copied));
            if (!converted) {
                return fail(converted.error());
            }
            return environment_common::normalize_directory_path(*converted);
        }

        full_path.resize(static_cast<std::size_t>(copied));
    }

    return fail(errc::temporarily_unavailable);
}

inline std::vector<std::wstring> get_pathext_extensions() {
    std::vector<std::wstring> extensions;
    const ::DWORD len = ::GetEnvironmentVariableW(L"PATHEXT", nullptr, 0U);
    if (len > 0U) {
        std::vector<wchar_t> buffer(len);
        const ::DWORD copied = ::GetEnvironmentVariableW(L"PATHEXT", buffer.data(), len);
        if (copied > 0U && copied < len) {
            std::wstring_view ext_str(buffer.data(), copied);
            std::size_t start = 0U;
            while (start < ext_str.size()) {
                const std::size_t sep = ext_str.find(L';', start);
                const std::wstring_view token = (sep == std::wstring_view::npos)
                                                    ? ext_str.substr(start)
                                                    : ext_str.substr(start, sep - start);
                if (!token.empty()) {
                    extensions.emplace_back(token);
                }
                if (sep == std::wstring_view::npos) {
                    break;
                }
                start = sep + 1U;
            }
        }
    }

    if (extensions.empty()) {
        extensions.push_back(L".COM");
        extensions.push_back(L".EXE");
        extensions.push_back(L".BAT");
        extensions.push_back(L".CMD");
    }
    return extensions;
}

inline result<std::string> check_candidate_file_with_extensions(
    const std::wstring& base_path,
    bool filename_has_extension,
    const std::vector<std::wstring>& pathext_list) {

    if (filename_has_extension && is_valid_executable_file(base_path.c_str())) {
        return make_windows_absolute_path(base_path.c_str());
    }

    for (const auto& ext : pathext_list) {
        std::wstring candidate = base_path;
        candidate.append(ext);
        if (is_valid_executable_file(candidate.c_str())) {
            return make_windows_absolute_path(candidate.c_str());
        }
    }

    if (!filename_has_extension && is_valid_executable_file(base_path.c_str())) {
        return make_windows_absolute_path(base_path.c_str());
    }

    return fail(errc::not_found);
}

inline result<std::string> find_executable(std::string_view name) {
    if (name.empty()) {
        return fail(errc::invalid_argument);
    }
    if (name.find('\0') != std::string_view::npos) {
        return fail(errc::invalid_argument);
    }
    if (!is_valid_utf8(name)) {
        return fail(errc::invalid_encoding);
    }

    const result<std::wstring> wide_name = utf8_to_wide(name);
    if (!wide_name) {
        return fail(wide_name.error());
    }

    const std::size_t last_slash = name.find_last_of("/\\");
    const std::string_view filename = (last_slash == std::string_view::npos)
                                          ? name
                                          : name.substr(last_slash + 1U);
    const bool filename_has_ext = filename.find('.') != std::string_view::npos;
    const std::vector<std::wstring> pathext_list = get_pathext_extensions();

    // If name contains directory separators, search directly without searching PATH
    if (last_slash != std::string_view::npos) {
        return check_candidate_file_with_extensions(
            *wide_name, filename_has_ext, pathext_list);
    }

    // Explicitly search directories listed in %PATH%
    const ::DWORD path_len = ::GetEnvironmentVariableW(L"PATH", nullptr, 0U);
    if (path_len == 0U) {
        return fail(errc::not_found);
    }

    std::vector<wchar_t> path_buf(path_len);
    const ::DWORD copied = ::GetEnvironmentVariableW(L"PATH", path_buf.data(), path_len);
    if (copied == 0U || copied >= path_len) {
        return fail(errc::not_found);
    }

    std::wstring_view path_str(path_buf.data(), copied);
    bool done = false;
    while (!done) {
        const std::size_t sep = path_str.find(L';');
        std::wstring_view dir = (sep == std::wstring_view::npos)
                                    ? path_str
                                    : path_str.substr(0, sep);

        // Strip enclosing quotes if present (e.g. "C:\Program Files\...")
        if (dir.size() >= 2U && dir.front() == L'"' && dir.back() == L'"') {
            dir = dir.substr(1, dir.size() - 2U);
        }

        if (!dir.empty()) {
            std::wstring candidate_base;
            candidate_base.reserve(dir.size() + 1U + wide_name->size());
            candidate_base.append(dir);
            if (candidate_base.back() != L'\\' && candidate_base.back() != L'/') {
                candidate_base.push_back(L'\\');
            }
            candidate_base.append(*wide_name);

            const auto found = check_candidate_file_with_extensions(
                candidate_base, filename_has_ext, pathext_list);
            if (found) {
                return found;
            }
        }

        if (sep == std::wstring_view::npos) {
            done = true;
        } else {
            path_str.remove_prefix(sep + 1U);
        }
    }

    return fail(errc::not_found);
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
