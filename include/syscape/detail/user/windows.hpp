#ifndef SYSCAPE_DETAIL_USER_WINDOWS_HPP
#define SYSCAPE_DETAIL_USER_WINDOWS_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

#include <windows.h>
#include <lmcons.h>
#include <shlobj.h>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace user_backend {

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

inline std::error_code last_error() noexcept {
    return std::error_code(static_cast<int>(::GetLastError()),
                           std::system_category());
}

/// Error category for HRESULT values outside the Win32 facility.
///
/// The system category describes Win32 error codes, so raw non-Win32
/// HRESULTs keep their full value under this dedicated category instead of
/// being misinterpreted.
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

/// Returns the process-wide category for raw non-Win32 HRESULT values.
inline const std::error_category& hresult_category() noexcept {
    static const hresult_error_category category;
    return category;
}

/// Maps an HRESULT to an error code without losing diagnostic accuracy.
///
/// Win32-facility HRESULTs are narrowed to their underlying Win32 code under
/// the system category, which keeps standard error conditions such as
/// permission-denied comparable. Every other facility preserves its complete
/// HRESULT value under the dedicated internal category.
inline std::error_code map_hresult(::HRESULT value) noexcept {
    if (HRESULT_FACILITY(value) == FACILITY_WIN32) {
        return std::error_code(static_cast<int>(HRESULT_CODE(value)),
                               std::system_category());
    }
    return std::error_code(static_cast<int>(value), hresult_category());
}

/// Returns whether a native Windows home-directory path is absolute.
inline bool is_absolute_home_path(std::wstring_view value) {
    return std::filesystem::path(value).is_absolute();
}

class co_task_memory {
public:
    explicit co_task_memory(::PWSTR value) noexcept : value_(value) {}
    co_task_memory(const co_task_memory&) = delete;
    co_task_memory& operator=(const co_task_memory&) = delete;
    ~co_task_memory() {
        if (value_ != nullptr) { ::CoTaskMemFree(value_); }
    }

    ::PWSTR get() const noexcept { return value_; }

private:
    ::PWSTR value_;
};

/// Returns not_supported because Windows defines no POSIX-style numeric user
/// or group identifier concept.
///
/// Security identifiers exist but are structured platform values, so they are
/// not silently squeezed into the portable numeric contract.
inline result<std::uint32_t> real_user_id() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> effective_user_id() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> real_group_id() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> effective_group_id() {
    return fail(errc::not_supported);
}

/// Returns the login name of the user associated with the current thread.
inline result<std::string> user_name() {
    constexpr std::size_t maximum_size = 32U * 1024U;
    std::wstring value(static_cast<std::size_t>(UNLEN) + 1U, L'\0');
    for (;;) {
        ::DWORD size = static_cast<::DWORD>(value.size());
        if (::GetUserNameW(value.data(), &size) != FALSE) {
            // On success the size includes the terminating null character.
            const std::size_t length = static_cast<std::size_t>(size);
            if (length <= 1U || length > value.size()) {
                return fail(errc::malformed_data);
            }
            value.resize(length - 1U);
            break;
        }

        const ::DWORD error = ::GetLastError();
        const std::size_t required = static_cast<std::size_t>(size);
        if (error != ERROR_INSUFFICIENT_BUFFER) {
            return fail(std::error_code(static_cast<int>(error),
                                        std::system_category()));
        }
        if (required <= value.size() || required > maximum_size) {
            return fail(errc::value_too_large);
        }
        value.resize(required);
    }

    const result<std::string> converted = wide_to_utf8(value);
    if (!converted) { return converted; }
    if (converted->empty()) { return fail(errc::malformed_data); }
    return converted;
}

/// Returns the profile directory of the current user from the documented
/// known-folder interface.
inline result<std::string> home_directory() {
    ::PWSTR raw = nullptr;
    const ::HRESULT outcome =
        ::SHGetKnownFolderPath(::FOLDERID_Profile, 0U, nullptr, &raw);
    const co_task_memory guard(raw);
    if (outcome != S_OK) { return fail(map_hresult(outcome)); }

    const std::wstring value(guard.get() != nullptr ? guard.get() : L"");
    if (value.empty()) { return fail(errc::malformed_data); }
    if (!is_absolute_home_path(value)) {
        return fail(errc::malformed_data);
    }

    const result<std::string> converted = wide_to_utf8(value);
    if (!converted) { return converted; }
    if (converted->empty()) { return fail(errc::malformed_data); }
    return converted;
}

/// Returns not_supported because Windows exposes no recorded login shell
/// through an acceptable public source.
inline result<std::string> shell() {
    return fail(errc::not_supported);
}

} // namespace user_backend
} // namespace detail
} // namespace syscape

#endif
