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

#include <cstdint>
#include <cstring>
#include <string>
#include <system_error>
#include <vector>

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

} // namespace security_backend
} // namespace detail
} // namespace syscape

#endif
