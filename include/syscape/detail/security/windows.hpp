#ifndef SYSCAPE_DETAIL_SECURITY_WINDOWS_HPP
#define SYSCAPE_DETAIL_SECURITY_WINDOWS_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tbs.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <system_error>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace security_backend {

class loaded_module {
public:
    explicit loaded_module(const wchar_t* name) noexcept
        : value_(::LoadLibraryW(name)) {}
    loaded_module(const loaded_module&) = delete;
    loaded_module& operator=(const loaded_module&) = delete;
    ~loaded_module() {
        if (value_ != nullptr) {
            ::FreeLibrary(value_);
        }
    }
    HMODULE get() const noexcept { return value_; }

private:
    HMODULE value_;
};

class tbs_error_category_type final : public std::error_category {
public:
    const char* name() const noexcept override { return "windows.tbs"; }

    std::string message(int value) const override {
        std::uint32_t native_value = 0U;
        static_assert(
            sizeof(native_value) == sizeof(value),
            "Windows TBS error codes require a 32-bit int");
        std::memcpy(&native_value, &value, sizeof(native_value));
        return "TPM Base Services error " + std::to_string(native_value);
    }
};

inline const std::error_category& tbs_error_category() noexcept {
    static const tbs_error_category_type category;
    return category;
}

inline std::error_code make_tbs_error(TBS_RESULT result) noexcept {
    std::int32_t signed_value = 0;
    const std::uint32_t native_value = static_cast<std::uint32_t>(result);
    static_assert(
        sizeof(signed_value) == sizeof(native_value),
        "Windows TBS error codes require a 32-bit value");
    std::memcpy(&signed_value, &native_value, sizeof(signed_value));
    return std::error_code(static_cast<int>(signed_value), tbs_error_category());
}

/// Queries the UEFI Secure Boot enablement state on Windows.
inline result<::syscape::security::secure_boot_state> secure_boot() {
    BYTE value = 0;
    constexpr const wchar_t* var_name = L"SecureBoot";
    constexpr const wchar_t* guid = L"{8be4df61-93ca-11d2-aa0d-00e098032b8c}";

    const DWORD res = ::GetFirmwareEnvironmentVariableW(
        var_name, guid, &value, sizeof(value));

    if (res > 0) {
        if (value == 1) {
            return ::syscape::security::secure_boot_state::enabled;
        }
        if (value == 0) {
            return ::syscape::security::secure_boot_state::disabled;
        }
        return fail(errc::malformed_data);
    }

    const DWORD err = ::GetLastError();
    if (err == ERROR_ENVVAR_NOT_FOUND || err == ERROR_INVALID_FUNCTION) {
        // System booted in legacy BIOS or variable does not exist
        return fail(errc::not_supported);
    }
    if (err == ERROR_PRIVILEGE_NOT_HELD || err == ERROR_ACCESS_DENIED) {
        return fail(errc::permission_denied);
    }
    return fail(std::error_code(static_cast<int>(err), std::system_category()));
}

/// Queries whether UEFI Secure Boot is active and enforcing on Windows.
inline result<bool> is_secure_boot_enabled() {
    const auto res = secure_boot();
    if (!res) {
        return fail(res.error());
    }
    return *res == ::syscape::security::secure_boot_state::enabled;
}

/// Queries Trusted Platform Module (TPM) presence and version on Windows via TBS.
inline result<::syscape::security::tpm_info> tpm() {
    ::syscape::security::tpm_info info;
    info.present = false;
    info.version = ::syscape::security::tpm_version::none;

    const loaded_module tbs_module(L"tbs.dll");
    if (tbs_module.get() == nullptr) {
        const DWORD error = ::GetLastError();
        if (error == ERROR_MOD_NOT_FOUND || error == ERROR_DLL_NOT_FOUND) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(
            static_cast<int>(error), std::system_category()));
    }

    using get_device_info_function = TBS_RESULT(WINAPI*)(UINT32, PVOID);
    const FARPROC address =
        ::GetProcAddress(tbs_module.get(), "Tbsi_GetDeviceInfo");
    if (address == nullptr) {
        const DWORD error = ::GetLastError();
        if (error == ERROR_PROC_NOT_FOUND) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(
            static_cast<int>(error), std::system_category()));
    }
    const auto get_device_info =
        reinterpret_cast<get_device_info_function>(address);

    TPM_DEVICE_INFO dev_info{};
    const TBS_RESULT tbs_res = get_device_info(
        static_cast<UINT32>(sizeof(dev_info)), static_cast<void*>(&dev_info));

    if (tbs_res == TBS_SUCCESS) {
        info.present = true;
        info.device_id = "tpm0";
        if (dev_info.tpmVersion == TPM_VERSION_20) {
            info.version = ::syscape::security::tpm_version::v2_0;
            info.version_string = "2.0";
        } else if (dev_info.tpmVersion == TPM_VERSION_12) {
            info.version = ::syscape::security::tpm_version::v1_2;
            info.version_string = "1.2";
        } else {
            info.version = ::syscape::security::tpm_version::other;
            info.version_string = std::to_string(dev_info.tpmVersion);
        }
        return info;
    }

    if (tbs_res == TBS_E_TPM_NOT_FOUND) {
        return info;
    }

    if (tbs_res == TBS_E_ACCESS_DENIED) {
        return fail(errc::permission_denied);
    }

    if (tbs_res == TBS_E_SERVICE_NOT_RUNNING ||
        tbs_res == TBS_E_SERVICE_DISABLED ||
        tbs_res == TBS_E_SERVICE_START_PENDING) {
        return fail(errc::temporarily_unavailable);
    }
    if (tbs_res == TBS_E_IOERROR) {
        return fail(errc::io_error);
    }
    return fail(make_tbs_error(tbs_res));
}

/// Linux Security Modules are not supported on Windows.
inline result<std::vector<std::string>> security_modules() {
    return fail(errc::not_supported);
}

/// Linux kernel lockdown is not supported on Windows.
inline result<::syscape::security::lockdown_mode> lockdown() {
    return fail(errc::not_supported);
}

/// macOS System Integrity Protection is not supported on Windows.
inline result<bool> is_sip_enabled() {
    return fail(errc::not_supported);
}

/// Queries the Address Space Layout Randomization (ASLR) mode of the calling process on Windows.
inline result<::syscape::security::aslr_mode> aslr() {
    PROCESS_MITIGATION_ASLR_POLICY policy{};
    if (::GetProcessMitigationPolicy(
            ::GetCurrentProcess(),
            ProcessASLRPolicy,
            &policy,
            sizeof(policy))) {
        if (policy.EnableBottomUpRandomization &&
            policy.EnableForceRelocateImages &&
            policy.EnableHighEntropy) {
            return ::syscape::security::aslr_mode::full;
        }
        if (policy.EnableBottomUpRandomization ||
            policy.EnableForceRelocateImages) {
            return ::syscape::security::aslr_mode::partial;
        }
        if (policy.DisallowStrippedImages) {
            return ::syscape::security::aslr_mode::disabled;
        }
        return ::syscape::security::aslr_mode::disabled;
    }
    const DWORD err = ::GetLastError();
    if (err == ERROR_NOT_SUPPORTED || err == ERROR_INVALID_PARAMETER) {
        return fail(errc::not_supported);
    }
    if (err == ERROR_ACCESS_DENIED) {
        return fail(errc::permission_denied);
    }
    return fail(std::error_code(static_cast<int>(err), std::system_category()));
}

/// CPU hardware vulnerability table is not exposed via standard Win32 APIs.
inline result<std::vector<::syscape::security::cpu_vulnerability_entry>>
cpu_vulnerabilities() {
    return fail(errc::not_supported);
}

/// POSIX process capabilities are not supported on Windows.
inline result<::syscape::security::process_capabilities> capabilities() {
    return fail(errc::not_supported);
}

/// Queries process token privileges on Windows.
inline result<std::vector<::syscape::security::privilege_entry>> privileges() {
    HANDLE token_handle = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token_handle)) {
        const DWORD err = ::GetLastError();
        if (err == ERROR_ACCESS_DENIED) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(static_cast<int>(err), std::system_category()));
    }

    struct token_closer {
        HANDLE handle;
        ~token_closer() {
            if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
                ::CloseHandle(handle);
            }
        }
    } closer{token_handle};

    DWORD length_needed = 0U;
    if (!::GetTokenInformation(
            token_handle, TokenPrivileges, nullptr, 0U, &length_needed)) {
        const DWORD err = ::GetLastError();
        if (err != ERROR_INSUFFICIENT_BUFFER) {
            if (err == ERROR_ACCESS_DENIED) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(static_cast<int>(err), std::system_category()));
        }
    }
    if (length_needed == 0U) {
        return std::vector<::syscape::security::privilege_entry>();
    }

    std::vector<unsigned char> buffer(length_needed);
    if (!::GetTokenInformation(
            token_handle,
            TokenPrivileges,
            buffer.data(),
            length_needed,
            &length_needed)) {
        const DWORD err = ::GetLastError();
        if (err == ERROR_ACCESS_DENIED) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(static_cast<int>(err), std::system_category()));
    }

    const auto* privs =
        reinterpret_cast<const TOKEN_PRIVILEGES*>(buffer.data());
    std::vector<::syscape::security::privilege_entry> result;
    result.reserve(privs->PrivilegeCount);

    for (DWORD i = 0U; i < privs->PrivilegeCount; ++i) {
        const auto& item = privs->Privileges[i];
        DWORD name_len = 256U;
        std::vector<wchar_t> name_buf(name_len);
        LUID luid = item.Luid;
        if (!::LookupPrivilegeNameW(nullptr, &luid, name_buf.data(), &name_len)) {
            const DWORD err = ::GetLastError();
            if (err == ERROR_INSUFFICIENT_BUFFER && name_len > 0U) {
                name_buf.resize(name_len);
                if (!::LookupPrivilegeNameW(nullptr, &luid, name_buf.data(), &name_len)) {
                    const DWORD err2 = ::GetLastError();
                    return fail(std::error_code(static_cast<int>(err2), std::system_category()));
                }
            } else {
                return fail(std::error_code(static_cast<int>(err), std::system_category()));
            }
        }

        // Convert UTF-16 to UTF-8 using WC_ERR_INVALID_CHARS to reject malformed surrogate pairs
        const int utf8_len = ::WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, name_buf.data(), static_cast<int>(name_len),
            nullptr, 0, nullptr, nullptr);
        if (utf8_len <= 0) {
            return fail(errc::invalid_encoding);
        }
        std::string name_utf8(static_cast<std::size_t>(utf8_len), '\0');
        if (::WideCharToMultiByte(
                CP_UTF8, WC_ERR_INVALID_CHARS, name_buf.data(), static_cast<int>(name_len),
                name_utf8.data(), utf8_len, nullptr, nullptr) <= 0) {
            return fail(errc::invalid_encoding);
        }

        ::syscape::security::privilege_entry entry;
        entry.name = std::move(name_utf8);
        entry.enabled = (item.Attributes & SE_PRIVILEGE_ENABLED) != 0;
        entry.enabled_by_default =
            (item.Attributes & SE_PRIVILEGE_ENABLED_BY_DEFAULT) != 0;
        result.push_back(std::move(entry));
    }

    std::sort(result.begin(), result.end(),
              [](const ::syscape::security::privilege_entry& a,
                 const ::syscape::security::privilege_entry& b) noexcept {
                  return a.name < b.name;
              });

    return result;
}

/// Volume encryption queries on Windows require BitLocker APIs/WMI which are currently not supported.
inline result<::syscape::security::volume_encryption_info>
volume_encryption(std::string_view path) {
    if (path.empty()) {
        return fail(errc::invalid_argument);
    }
    if (!is_valid_utf8(path)) {
        return fail(errc::invalid_encoding);
    }
    if (path.find('\0') != std::string_view::npos) {
        return fail(errc::invalid_argument);
    }
    return fail(errc::not_supported);
}

/// Volume encryption enumeration on Windows requires BitLocker APIs/WMI which are currently not supported.
inline result<std::vector<::syscape::security::volume_encryption_info>>
encrypted_volumes() {
    return fail(errc::not_supported);
}

} // namespace security_backend
} // namespace detail
} // namespace syscape

#endif
