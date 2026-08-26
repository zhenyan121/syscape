#ifndef SYSCAPE_DETAIL_SOFTWARE_WINDOWS_HPP
#define SYSCAPE_DETAIL_SOFTWARE_WINDOWS_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <winsvc.h>
#include <psapi.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <syscape/detail/software/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace software_backend {
namespace windows_impl {

inline result<std::string> wide_to_utf8(std::wstring_view value) {
    static_assert(sizeof(wchar_t) == sizeof(char16_t),
                  "The Windows backend requires 16-bit wchar_t");
    std::u16string converted;
    converted.reserve(value.size());
    for (wchar_t unit : value) {
        converted.push_back(static_cast<char16_t>(unit));
    }
    return detail::utf16_to_utf8(converted);
}

class scm_handle {
public:
    explicit scm_handle(SC_HANDLE handle = nullptr) noexcept : handle_(handle) {}
    scm_handle(const scm_handle&) = delete;
    scm_handle& operator=(const scm_handle&) = delete;
    ~scm_handle() {
        if (handle_ != nullptr) {
            ::CloseServiceHandle(handle_);
        }
    }
    SC_HANDLE get() const noexcept { return handle_; }
    explicit operator bool() const noexcept { return handle_ != nullptr; }

private:
    SC_HANDLE handle_;
};

class reg_handle {
public:
    explicit reg_handle(HKEY key = nullptr) noexcept : key_(key) {}
    reg_handle(const reg_handle&) = delete;
    reg_handle& operator=(const reg_handle&) = delete;
    ~reg_handle() {
        if (key_ != nullptr) {
            ::RegCloseKey(key_);
        }
    }
    HKEY get() const noexcept { return key_; }
    explicit operator bool() const noexcept { return key_ != nullptr; }

private:
    HKEY key_;
};

inline software_common::service_state map_service_state(DWORD state) noexcept {
    switch (state) {
    case SERVICE_RUNNING:
        return software_common::service_state::running;
    case SERVICE_STOPPED:
        return software_common::service_state::stopped;
    case SERVICE_PAUSED:
        return software_common::service_state::paused;
    case SERVICE_START_PENDING:
        return software_common::service_state::starting;
    case SERVICE_STOP_PENDING:
        return software_common::service_state::stopping;
    case SERVICE_PAUSE_PENDING:
    case SERVICE_CONTINUE_PENDING:
        return software_common::service_state::running;
    default:
        return software_common::service_state::unknown;
    }
}

inline software_common::service_startup map_service_startup(DWORD start_type) noexcept {
    switch (start_type) {
    case SERVICE_BOOT_START:
    case SERVICE_SYSTEM_START:
    case SERVICE_AUTO_START:
        return software_common::service_startup::automatic;
    case SERVICE_DEMAND_START:
        return software_common::service_startup::manual;
    case SERVICE_DISABLED:
        return software_common::service_startup::disabled;
    default:
        return software_common::service_startup::unknown;
    }
}

inline result<std::wstring> expand_environment_string(std::wstring_view value) {
    const std::wstring input(value);
    DWORD required = ::ExpandEnvironmentStringsW(input.c_str(), nullptr, 0);
    if (required == 0) {
        return fail(std::error_code(static_cast<int>(::GetLastError()), std::system_category()));
    }

    for (int attempt = 0; attempt < 3; ++attempt) {
        std::vector<wchar_t> buffer(required, L'\0');
        const DWORD written = ::ExpandEnvironmentStringsW(
            input.c_str(), buffer.data(), static_cast<DWORD>(buffer.size()));
        if (written == 0) {
            return fail(std::error_code(static_cast<int>(::GetLastError()), std::system_category()));
        }
        if (written <= static_cast<DWORD>(buffer.size())) {
            if (written == 0 || buffer[written - 1] != L'\0') {
                return fail(errc::malformed_data);
            }
            return std::wstring(buffer.data(), written - 1);
        }
        required = written;
    }
    return fail(errc::temporarily_unavailable);
}

inline result<std::optional<std::wstring>> read_registry_string(HKEY key, const wchar_t* value_name) {
    DWORD type = 0;
    DWORD bytes = 0;
    LSTATUS status = ::RegQueryValueExW(key, value_name, nullptr, &type, nullptr, &bytes);
    if (status == ERROR_FILE_NOT_FOUND) {
        return std::optional<std::wstring>{std::nullopt};
    }
    if (status != ERROR_SUCCESS) {
        return fail(std::error_code(static_cast<int>(status), std::system_category()));
    }
    if (type != REG_SZ && type != REG_EXPAND_SZ) {
        return fail(errc::malformed_data);
    }
    if ((bytes % sizeof(wchar_t)) != 0) {
        return fail(errc::malformed_data);
    }
    if (bytes == 0) {
        return std::optional<std::wstring>{std::wstring{}};
    }

    std::vector<wchar_t> buffer;
    for (int attempt = 0; attempt < 3; ++attempt) {
        buffer.resize((bytes / sizeof(wchar_t)) + 1, L'\0');
        DWORD read_bytes = bytes;
        type = 0;
        status = ::RegQueryValueExW(key, value_name, nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &read_bytes);
        if (status == ERROR_SUCCESS) {
            if (type != REG_SZ && type != REG_EXPAND_SZ) {
                return fail(errc::malformed_data);
            }
            if ((read_bytes % sizeof(wchar_t)) != 0) {
                return fail(errc::malformed_data);
            }
            std::size_t char_count = read_bytes / sizeof(wchar_t);
            while (char_count > 0 && buffer[char_count - 1] == L'\0') {
                --char_count;
            }
            std::wstring value(buffer.data(), char_count);
            if (type == REG_EXPAND_SZ) {
                const auto expanded = expand_environment_string(value);
                if (!expanded) {
                    return fail(expanded.error());
                }
                value = *expanded;
            }
            return std::optional<std::wstring>{std::move(value)};
        }
        if (status == ERROR_MORE_DATA) {
            bytes = (read_bytes > bytes ? read_bytes : bytes * 2) + 32;
            continue;
        }
        return fail(std::error_code(static_cast<int>(status), std::system_category()));
    }
    return fail(errc::temporarily_unavailable);
}

inline result<std::wstring> get_device_driver_name(LPVOID image_base) {
    std::vector<wchar_t> buffer(MAX_PATH);
    for (int attempt = 0; attempt < 5; ++attempt) {
        const DWORD len = ::GetDeviceDriverBaseNameW(image_base, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (len == 0) {
            return fail(std::error_code(static_cast<int>(::GetLastError()), std::system_category()));
        }
        if (len < buffer.size() - 1) {
            return std::wstring(buffer.data(), len);
        }
        buffer.resize(buffer.size() * 2);
    }
    return fail(errc::value_too_large);
}

inline result<std::optional<std::wstring>> get_device_driver_path(LPVOID image_base) {
    std::vector<wchar_t> buffer(MAX_PATH);
    for (int attempt = 0; attempt < 5; ++attempt) {
        const DWORD len = ::GetDeviceDriverFileNameW(image_base, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (len == 0) {
            const DWORD error = ::GetLastError();
            if (error == ERROR_ACCESS_DENIED) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(static_cast<int>(error), std::system_category()));
        }
        if (len < buffer.size() - 1) {
            return std::optional<std::wstring>{std::wstring(buffer.data(), len)};
        }
        buffer.resize(buffer.size() * 2);
    }
    return fail(errc::value_too_large);
}

inline result<std::vector<software_common::service_record>> query_services_from_scm(SC_HANDLE scm, DWORD service_type) {
    DWORD resume_handle = 0;
    std::vector<software_common::service_record> records;
    constexpr DWORD kMaxEnumBufferSize = 256U * 1024U; // Windows API upper limit for EnumServicesStatusExW

    for (;;) {
        DWORD bytes_needed = 0;
        DWORD services_returned = 0;

        const BOOL probe_ok = ::EnumServicesStatusExW(
            scm,
            SC_ENUM_PROCESS_INFO,
            service_type,
            SERVICE_STATE_ALL,
            nullptr,
            0,
            &bytes_needed,
            &services_returned,
            &resume_handle,
            nullptr);

        if (!probe_ok) {
            const DWORD probe_error = ::GetLastError();
            if (probe_error != ERROR_MORE_DATA && probe_error != ERROR_INSUFFICIENT_BUFFER) {
                return fail(std::error_code(static_cast<int>(probe_error), std::system_category()));
            }
        }

        if (bytes_needed == 0 && services_returned == 0 && resume_handle == 0) {
            break;
        }

        DWORD alloc_size = (bytes_needed > kMaxEnumBufferSize || bytes_needed == 0) ? kMaxEnumBufferSize : bytes_needed;
        std::vector<BYTE> buffer(alloc_size);

        const BOOL success = ::EnumServicesStatusExW(
            scm,
            SC_ENUM_PROCESS_INFO,
            service_type,
            SERVICE_STATE_ALL,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            &bytes_needed,
            &services_returned,
            &resume_handle,
            nullptr);

        if (!success) {
            const DWORD error = ::GetLastError();
            if (error != ERROR_MORE_DATA) {
                return fail(std::error_code(static_cast<int>(error), std::system_category()));
            }
        }

        const auto* services = reinterpret_cast<const ENUM_SERVICE_STATUS_PROCESSW*>(buffer.data());
        for (DWORD i = 0; i < services_returned; ++i) {
            if (services[i].lpServiceName == nullptr) {
                continue;
            }

            const auto name_utf8 = wide_to_utf8(services[i].lpServiceName);
            if (!name_utf8) {
                continue;
            }

            software_common::service_record rec;
            rec.name = *name_utf8;

            if (services[i].lpDisplayName != nullptr) {
                const auto display_utf8 = wide_to_utf8(services[i].lpDisplayName);
                if (!display_utf8) {
                    continue;
                }
                rec.display_name = *display_utf8;
            }

            rec.state = map_service_state(services[i].ServiceStatusProcess.dwCurrentState);
            if (services[i].ServiceStatusProcess.dwProcessId != 0) {
                rec.pid = static_cast<std::uint32_t>(services[i].ServiceStatusProcess.dwProcessId);
            }

            scm_handle svc(::OpenServiceW(scm, services[i].lpServiceName, SERVICE_QUERY_CONFIG));
            if (!svc) {
                records.push_back(std::move(rec));
                continue;
            }

            DWORD config_needed = 0;
            const BOOL config_probe = ::QueryServiceConfigW(svc.get(), nullptr, 0, &config_needed);
            DWORD config_err = ::GetLastError();
            if (!config_probe && config_err != ERROR_INSUFFICIENT_BUFFER) {
                records.push_back(std::move(rec));
                continue;
            }

            if (config_needed > 0) {
                bool config_read = false;
                bool config_valid = true;
                for (int attempt = 0; attempt < 3; ++attempt) {
                    std::vector<BYTE> config_buffer(config_needed);
                    auto* config = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(config_buffer.data());
                    if (::QueryServiceConfigW(svc.get(), config, config_needed, &config_needed)) {
                        rec.startup_type = map_service_startup(config->dwStartType);
                        if (config->lpBinaryPathName != nullptr) {
                            const auto bin_utf8 = wide_to_utf8(config->lpBinaryPathName);
                            if (!bin_utf8) {
                                config_valid = false;
                                break;
                            }
                            if (!bin_utf8->empty()) {
                                rec.executable_path = *bin_utf8;
                            }
                        }
                        config_read = true;
                        break;
                    }
                    config_err = ::GetLastError();
                    if (config_err != ERROR_INSUFFICIENT_BUFFER) {
                        config_valid = false;
                        break;
                    }
                }
                if (!config_read || !config_valid) {
                    records.push_back(std::move(rec));
                    continue;
                }

                if (rec.startup_type == software_common::service_startup::automatic) {
                    DWORD delayed_needed = 0;
                    const BOOL delayed_probe = ::QueryServiceConfig2W(svc.get(), SERVICE_CONFIG_DELAYED_AUTO_START_INFO, nullptr, 0, &delayed_needed);
                    DWORD delayed_err = ::GetLastError();
                    if (!delayed_probe && delayed_err == ERROR_INSUFFICIENT_BUFFER) {
                        if (delayed_needed == 0) {
                            records.push_back(std::move(rec));
                            continue;
                        }
                        bool delayed_read = false;
                        for (int attempt = 0; attempt < 3; ++attempt) {
                            std::vector<BYTE> delayed_buf(delayed_needed);
                            auto* delayed_info = reinterpret_cast<SERVICE_DELAYED_AUTO_START_INFO*>(delayed_buf.data());
                            if (::QueryServiceConfig2W(svc.get(), SERVICE_CONFIG_DELAYED_AUTO_START_INFO, delayed_buf.data(), delayed_needed, &delayed_needed)) {
                                if (delayed_info->fDelayedAutostart) {
                                    rec.startup_type = software_common::service_startup::delayed_automatic;
                                }
                                delayed_read = true;
                                break;
                            }
                            delayed_err = ::GetLastError();
                            if (delayed_err != ERROR_INSUFFICIENT_BUFFER) {
                                break;
                            }
                        }
                        if (!delayed_read) {
                            records.push_back(std::move(rec));
                            continue;
                        }
                    }
                }
            }

            records.push_back(std::move(rec));
        }

        if (resume_handle == 0) {
            break;
        }
    }

    return records;
}

inline result<void> query_uninstall_registry_key(
    HKEY root,
    const wchar_t* subkey_path,
    std::unordered_set<std::string>& seen_keys,
    std::vector<software_common::package_record>& out,
    bool& key_opened) {
    HKEY key = nullptr;
    const LSTATUS status = ::RegOpenKeyExW(root, subkey_path, 0, KEY_READ, &key);
    if (status == ERROR_FILE_NOT_FOUND) {
        return {};
    }
    if (status != ERROR_SUCCESS) {
        return fail(std::error_code(static_cast<int>(status), std::system_category()));
    }
    key_opened = true;
    reg_handle owned_key(key);

    DWORD subkeys_count = 0;
    DWORD max_subkey_len = 0;
    const LSTATUS info_status = ::RegQueryInfoKeyW(
        owned_key.get(),
        nullptr,
        nullptr,
        nullptr,
        &subkeys_count,
        &max_subkey_len,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr);
    if (info_status != ERROR_SUCCESS) {
        return fail(std::error_code(static_cast<int>(info_status), std::system_category()));
    }

    std::vector<wchar_t> subkey_name(max_subkey_len + 1);

    for (DWORD index = 0; index < subkeys_count; ++index) {
        DWORD name_len = static_cast<DWORD>(subkey_name.size());
        LSTATUS enum_res = ::RegEnumKeyExW(
            owned_key.get(),
            index,
            subkey_name.data(),
            &name_len,
            nullptr,
            nullptr,
            nullptr,
            nullptr);

        if (enum_res == ERROR_MORE_DATA) {
            subkey_name.resize(subkey_name.size() * 2);
            name_len = static_cast<DWORD>(subkey_name.size());
            enum_res = ::RegEnumKeyExW(
                owned_key.get(),
                index,
                subkey_name.data(),
                &name_len,
                nullptr,
                nullptr,
                nullptr,
                nullptr);
        }

        if (enum_res == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (enum_res == ERROR_FILE_NOT_FOUND) {
            continue; // Concurrent deletion
        }
        if (enum_res != ERROR_SUCCESS) {
            return fail(std::error_code(static_cast<int>(enum_res), std::system_category()));
        }

        HKEY app_key = nullptr;
        const LSTATUS open_app_status = ::RegOpenKeyExW(owned_key.get(), subkey_name.data(), 0, KEY_READ, &app_key);
        if (open_app_status == ERROR_FILE_NOT_FOUND) {
            continue; // Concurrent deletion
        }
        if (open_app_status != ERROR_SUCCESS) {
            continue;
        }
        reg_handle owned_app(app_key);

        const auto display_name_res = read_registry_string(owned_app.get(), L"DisplayName");
        if (!display_name_res) {
            continue;
        }
        if (!*display_name_res || (*display_name_res)->empty()) {
            continue;
        }

        const auto name_utf8 = wide_to_utf8(**display_name_res);
        if (!name_utf8) {
            continue;
        }
        if (name_utf8->empty()) {
            continue;
        }

        software_common::package_record rec;
        rec.name = *name_utf8;
        rec.format = software_common::package_format::windows_installer;

        // DisplayVersion
        const auto version_res = read_registry_string(owned_app.get(), L"DisplayVersion");
        if (!version_res) {
            continue;
        }
        if (*version_res && !(*version_res)->empty()) {
            const auto ver_utf8 = wide_to_utf8(**version_res);
            if (!ver_utf8) {
                continue;
            }
            rec.version = *ver_utf8;
        }

        // Publisher
        const auto publisher_res = read_registry_string(owned_app.get(), L"Publisher");
        if (!publisher_res) {
            continue;
        }
        if (*publisher_res && !(*publisher_res)->empty()) {
            const auto pub_utf8 = wide_to_utf8(**publisher_res);
            if (!pub_utf8) {
                continue;
            }
            rec.publisher = *pub_utf8;
        }

        // InstallLocation
        const auto location_res = read_registry_string(owned_app.get(), L"InstallLocation");
        if (!location_res) {
            continue;
        }
        if (*location_res && !(*location_res)->empty()) {
            const auto loc_utf8 = wide_to_utf8(**location_res);
            if (!loc_utf8) {
                continue;
            }
            rec.install_location = *loc_utf8;
        }

        // Comments / Description
        const auto comments_res = read_registry_string(owned_app.get(), L"Comments");
        if (!comments_res) {
            continue;
        }
        if (*comments_res && !(*comments_res)->empty()) {
            const auto comm_utf8 = wide_to_utf8(**comments_res);
            if (!comm_utf8) {
                continue;
            }
            rec.description = *comm_utf8;
        }

        const std::string dedup_key = software_common::make_package_dedup_key(rec);
        if (seen_keys.find(dedup_key) != seen_keys.end()) {
            continue;
        }
        seen_keys.insert(dedup_key);

        out.push_back(std::move(rec));
    }

    return {};
}

} // namespace windows_impl

inline result<std::vector<software_common::service_record>> services() {
    windows_impl::scm_handle scm(::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_CONNECT));
    if (!scm) {
        const DWORD scm_err = ::GetLastError();
        if (scm_err == ERROR_ACCESS_DENIED) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(static_cast<int>(scm_err), std::system_category()));
    }

    auto res = windows_impl::query_services_from_scm(scm.get(), SERVICE_WIN32);
    if (res) {
        std::sort(res->begin(), res->end(), software_common::compare_services);
    }
    return res;
}

inline result<software_common::service_record> find_service(std::string_view name) {
    if (name.empty()) {
        return fail(errc::invalid_argument);
    }
    const result<std::vector<software_common::service_record>> all = services();
    if (!all) {
        return fail(all.error());
    }
    for (const auto& item : *all) {
        if (item.name == name) {
            return item;
        }
    }
    return fail(errc::not_found);
}

inline result<std::vector<software_common::driver_record>> loaded_drivers() {
    DWORD bytes_needed = 0;
    if (!::EnumDeviceDrivers(nullptr, 0, &bytes_needed)) {
        const DWORD enum_err = ::GetLastError();
        if (enum_err == ERROR_ACCESS_DENIED) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(static_cast<int>(enum_err), std::system_category()));
    }

    if (bytes_needed == 0) {
        return std::vector<software_common::driver_record>{};
    }

    std::vector<LPVOID> drivers_base;
    bool snapshot_acquired = false;

    for (int attempt = 0; attempt < 5; ++attempt) {
        drivers_base.resize(bytes_needed / sizeof(LPVOID));
        const DWORD cb_size = static_cast<DWORD>(drivers_base.size() * sizeof(LPVOID));
        DWORD cb_needed = 0;
        if (!::EnumDeviceDrivers(drivers_base.data(), cb_size, &cb_needed)) {
            const DWORD err = ::GetLastError();
            if (err == ERROR_ACCESS_DENIED) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(static_cast<int>(err), std::system_category()));
        }

        if (cb_needed <= cb_size) {
            const std::size_t count = cb_needed / sizeof(LPVOID);
            drivers_base.resize(count);
            snapshot_acquired = true;
            break;
        }
        bytes_needed = cb_needed;
    }

    if (!snapshot_acquired) {
        return fail(errc::temporarily_unavailable);
    }

    if (drivers_base.empty()) {
        return std::vector<software_common::driver_record>{};
    }

    std::size_t non_null_count = 0;
    std::vector<software_common::driver_record> records;
    records.reserve(drivers_base.size());

    for (std::size_t i = 0; i < drivers_base.size(); ++i) {
        if (drivers_base[i] == nullptr) {
            continue;
        }
        ++non_null_count;

        const auto base_name_res = windows_impl::get_device_driver_name(drivers_base[i]);
        if (!base_name_res) {
            return fail(base_name_res.error());
        }

        const auto name_utf8 = windows_impl::wide_to_utf8(*base_name_res);
        if (!name_utf8) {
            return fail(name_utf8.error());
        }
        if (name_utf8->empty()) {
            return fail(errc::malformed_data);
        }

        software_common::driver_record rec;
        rec.name = *name_utf8;
        rec.state = software_common::driver_state::loaded;

        const auto file_path_res = windows_impl::get_device_driver_path(drivers_base[i]);
        if (!file_path_res) {
            return fail(file_path_res.error());
        }
        if (*file_path_res) {
            const auto path_utf8 = windows_impl::wide_to_utf8(**file_path_res);
            if (!path_utf8) {
                return fail(path_utf8.error());
            }
            if (!path_utf8->empty()) {
                rec.path = *path_utf8;
            }
        }

        records.push_back(std::move(rec));
    }

    if (!drivers_base.empty() && non_null_count == 0) {
        return fail(errc::permission_denied);
    }

    std::sort(records.begin(), records.end(), software_common::compare_drivers);
    return records;
}

inline result<software_common::driver_record> find_driver(std::string_view name) {
    if (name.empty()) {
        return fail(errc::invalid_argument);
    }
    const result<std::vector<software_common::driver_record>> all = loaded_drivers();
    if (!all) {
        return fail(all.error());
    }
    for (const auto& item : *all) {
        if (item.name == name) {
            return item;
        }
    }
    return fail(errc::not_found);
}

inline result<std::vector<software_common::package_record>> installed_packages() {
    std::unordered_set<std::string> seen_keys;
    std::vector<software_common::package_record> pkgs;
    bool any_opened = false;

    static const wchar_t* const paths[] = {
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"
    };

    for (const wchar_t* path : paths) {
        auto res = windows_impl::query_uninstall_registry_key(HKEY_LOCAL_MACHINE, path, seen_keys, pkgs, any_opened);
        if (!res) {
            return fail(res.error());
        }
    }
    auto res_cu = windows_impl::query_uninstall_registry_key(
        HKEY_CURRENT_USER,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        seen_keys,
        pkgs,
        any_opened);
    if (!res_cu) {
        return fail(res_cu.error());
    }

    if (!any_opened && pkgs.empty()) {
        return fail(errc::not_supported);
    }

    std::sort(pkgs.begin(), pkgs.end(), software_common::compare_packages);
    return pkgs;
}

inline result<software_common::package_record> find_package(std::string_view name) {
    if (name.empty()) {
        return fail(errc::invalid_argument);
    }
    const result<std::vector<software_common::package_record>> all = installed_packages();
    if (!all) {
        return fail(all.error());
    }
    for (const auto& item : *all) {
        if (item.name == name) {
            return item;
        }
    }
    return fail(errc::not_found);
}

} // namespace software_backend
} // namespace detail
} // namespace syscape

#endif
