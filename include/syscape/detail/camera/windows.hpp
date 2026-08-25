#ifndef SYSCAPE_DETAIL_CAMERA_WINDOWS_HPP
#define SYSCAPE_DETAIL_CAMERA_WINDOWS_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <setupapi.h>
#include <winternl.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <syscape/camera.hpp>
#include <syscape/detail/camera/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace camera_backend {

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

class devinfo_handle {
public:
    explicit devinfo_handle(HDEVINFO handle) noexcept : handle_(handle) {}
    devinfo_handle(const devinfo_handle&) = delete;
    devinfo_handle& operator=(const devinfo_handle&) = delete;
    ~devinfo_handle() {
        if (handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr) {
            ::SetupDiDestroyDeviceInfoList(handle_);
        }
    }
    bool valid() const noexcept {
        return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr;
    }
    HDEVINFO get() const noexcept { return handle_; }

private:
    HDEVINFO handle_;
};

inline std::error_code windows_error(DWORD value) noexcept {
    if (value == ERROR_SUCCESS) {
        return make_error_code(errc::io_error);
    }
    if (value > static_cast<DWORD>((std::numeric_limits<int>::max)())) {
        return make_error_code(errc::io_error);
    }
    return std::error_code(static_cast<int>(value), std::system_category());
}

inline bool
camera_interface_is_available_for_version(DWORD major_version,
                                          DWORD build_number) noexcept {
    constexpr DWORD first_camera_interface_build = 17134U;
    return major_version > 10U ||
           (major_version == 10U &&
            build_number >= first_camera_interface_build);
}

inline result<bool> camera_interface_is_available() {
    const HMODULE module = ::GetModuleHandleW(L"ntdll.dll");
    if (module == nullptr) {
        return fail(windows_error(::GetLastError()));
    }
    const FARPROC address = ::GetProcAddress(module, "RtlGetVersion");
    if (address == nullptr) {
        return fail(windows_error(::GetLastError()));
    }
    using function_type = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    const auto function = reinterpret_cast<function_type>(address);
    RTL_OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (function(&version) < 0) {
        return fail(errc::io_error);
    }
    return camera_interface_is_available_for_version(version.dwMajorVersion,
                                                     version.dwBuildNumber);
}

inline result<std::optional<std::string>>
get_device_property_string(HDEVINFO dev_info, SP_DEVINFO_DATA* dev_data,
                           DWORD property_id) {
    DWORD property_type = 0U;
    DWORD required_size = 0U;
    if (::SetupDiGetDeviceRegistryPropertyW(dev_info, dev_data, property_id,
                                            &property_type, nullptr, 0U,
                                            &required_size)) {
        return fail(errc::malformed_data);
    }
    const DWORD size_error = ::GetLastError();
    if (size_error == ERROR_INVALID_DATA) {
        return std::optional<std::string>{};
    }
    if (size_error != ERROR_INSUFFICIENT_BUFFER) {
        return fail(windows_error(size_error));
    }
    if (required_size == 0U) {
        return fail(errc::malformed_data);
    }
    if (required_size % sizeof(wchar_t) != 0U) {
        return fail(errc::malformed_data);
    }
    std::vector<wchar_t> buffer(required_size / sizeof(wchar_t));
    if (!::SetupDiGetDeviceRegistryPropertyW(
            dev_info, dev_data, property_id, &property_type,
            reinterpret_cast<PBYTE>(buffer.data()), required_size, nullptr)) {
        return fail(windows_error(::GetLastError()));
    }
    if (property_type != REG_SZ && property_type != REG_MULTI_SZ) {
        return fail(errc::malformed_data);
    }
    std::size_t length = 0U;
    while (length < buffer.size() && buffer[length] != L'\0') {
        ++length;
    }
    if (length == buffer.size()) {
        return fail(errc::malformed_data);
    }
    const std::wstring_view wstr(buffer.data(), length);
    const auto utf8_res = wide_to_utf8(wstr);
    if (!utf8_res) {
        return fail(utf8_res.error());
    }
    return std::optional<std::string>{*utf8_res};
}

inline result<std::string> get_device_instance_id(HDEVINFO dev_info,
                                                  SP_DEVINFO_DATA* dev_data) {
    DWORD required_chars = 0U;
    if (::SetupDiGetDeviceInstanceIdW(dev_info, dev_data, nullptr, 0U,
                                      &required_chars)) {
        return fail(errc::malformed_data);
    }
    const DWORD size_error = ::GetLastError();
    if (size_error != ERROR_INSUFFICIENT_BUFFER || required_chars == 0U) {
        return fail(size_error == ERROR_INSUFFICIENT_BUFFER
                        ? make_error_code(errc::malformed_data)
                        : windows_error(size_error));
    }
    std::vector<wchar_t> buffer(required_chars);
    if (!::SetupDiGetDeviceInstanceIdW(dev_info, dev_data, buffer.data(),
                                       required_chars, nullptr)) {
        return fail(windows_error(::GetLastError()));
    }
    std::size_t length = 0U;
    while (length < buffer.size() && buffer[length] != L'\0') {
        ++length;
    }
    if (length == buffer.size()) {
        return fail(errc::malformed_data);
    }
    return wide_to_utf8(std::wstring_view(buffer.data(), length));
}

inline result<std::optional<::syscape::camera::camera_device_id>>
parse_windows_hardware_id(std::string_view hwid) {
    // Expected format: USB\VID_5986&PID_2175&REV_0003 or PCI\VEN_...
    ::syscape::camera::camera_device_id id;
    std::string normalized(hwid);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::toupper(value));
                   });
    const std::string_view normalized_view(normalized);

    const auto vid_pos = normalized_view.find("VID_");
    const auto ven_pos = normalized_view.find("VEN_");
    const std::size_t vpos = (vid_pos != std::string_view::npos) ? vid_pos + 4U
                             : (ven_pos != std::string_view::npos)
                                 ? ven_pos + 4U
                                 : std::string_view::npos;
    if (vpos != std::string_view::npos) {
        if (vpos > hwid.size() || hwid.size() - vpos < 4U) {
            return fail(errc::malformed_data);
        }
        const auto v_hex = camera_common::parse_hex_u16(hwid.substr(vpos, 4U));
        if (!v_hex) {
            return fail(v_hex.error());
        }
        id.vendor_id = *v_hex;
    }

    const auto pid_pos = normalized_view.find("PID_");
    const auto dev_pos = normalized_view.find("DEV_");
    const std::size_t ppos = (pid_pos != std::string_view::npos) ? pid_pos + 4U
                             : (dev_pos != std::string_view::npos)
                                 ? dev_pos + 4U
                                 : std::string_view::npos;
    if (ppos != std::string_view::npos) {
        if (ppos > hwid.size() || hwid.size() - ppos < 4U) {
            return fail(errc::malformed_data);
        }
        const auto p_hex = camera_common::parse_hex_u16(hwid.substr(ppos, 4U));
        if (!p_hex) {
            return fail(p_hex.error());
        }
        id.product_id = *p_hex;
    }

    const auto rev_pos = normalized_view.find("REV_");
    if (rev_pos != std::string_view::npos) {
        const std::size_t value_pos = rev_pos + 4U;
        if (value_pos > hwid.size() || hwid.size() - value_pos < 4U) {
            return fail(errc::malformed_data);
        }
        const auto r_hex =
            camera_common::parse_hex_u16(hwid.substr(rev_pos + 4U, 4U));
        if (!r_hex) {
            return fail(r_hex.error());
        }
        id.revision = *r_hex;
    }

    if (!id.vendor_id.has_value() && !id.product_id.has_value() &&
        !id.revision.has_value()) {
        return std::optional<::syscape::camera::camera_device_id>{};
    }
    return std::optional<::syscape::camera::camera_device_id>{id};
}

inline result<std::vector<::syscape::camera::camera_device>>
enumerate_windows_cameras() {
    const auto interface_available = camera_interface_is_available();
    if (!interface_available) {
        return fail(interface_available.error());
    }
    if (!*interface_available) {
        return fail(errc::not_supported);
    }

    // Use only the camera-specific device interface. The broader image and
    // capture classes can also contain scanners, audio endpoints, and filters.
    static const GUID camera_guid = {
        0x24E552D7U,
        0x6523U,
        0x47F7U,
        {0xA6U, 0x47U, 0xD3U, 0x46U, 0x5BU, 0xF1U, 0xF5U, 0xCAU}};

    std::vector<::syscape::camera::camera_device> devices;
    const devinfo_handle dev_info(::SetupDiGetClassDevsW(
        &camera_guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
    if (!dev_info.valid()) {
        return fail(windows_error(::GetLastError()));
    }

    for (DWORD i = 0U;; ++i) {
        SP_DEVICE_INTERFACE_DATA iface_data{};
        iface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        if (!::SetupDiEnumDeviceInterfaces(dev_info.get(), nullptr,
                                           &camera_guid, i, &iface_data)) {
            const DWORD enum_error = ::GetLastError();
            if (enum_error == ERROR_NO_MORE_ITEMS) {
                break;
            }
            return fail(windows_error(enum_error));
        }
        DWORD detail_size = 0U;
        if (::SetupDiGetDeviceInterfaceDetailW(dev_info.get(), &iface_data,
                                               nullptr, 0U, &detail_size,
                                               nullptr)) {
            return fail(errc::malformed_data);
        }
        const DWORD detail_error = ::GetLastError();
        if (detail_error != ERROR_INSUFFICIENT_BUFFER ||
            detail_size < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) {
            return fail(detail_error == ERROR_INSUFFICIENT_BUFFER
                            ? make_error_code(errc::malformed_data)
                            : windows_error(detail_error));
        }

        std::vector<BYTE> detail_buffer(detail_size);
        auto* detail_data =
            reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(
                detail_buffer.data());
        detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        SP_DEVINFO_DATA dev_data{};
        dev_data.cbSize = sizeof(SP_DEVINFO_DATA);

        if (!::SetupDiGetDeviceInterfaceDetailW(dev_info.get(), &iface_data,
                                                detail_data, detail_size,
                                                nullptr, &dev_data)) {
            return fail(windows_error(::GetLastError()));
        }

        const std::size_t path_capacity =
            (static_cast<std::size_t>(detail_size) -
             offsetof(SP_DEVICE_INTERFACE_DETAIL_DATA_W, DevicePath)) /
            sizeof(wchar_t);
        std::size_t path_length = 0U;
        while (path_length < path_capacity &&
               detail_data->DevicePath[path_length] != L'\0') {
            ++path_length;
        }
        if (path_length == path_capacity) {
            return fail(errc::malformed_data);
        }
        const std::wstring_view device_path_w(detail_data->DevicePath,
                                              path_length);
        const auto path_utf8 = wide_to_utf8(device_path_w);
        if (!path_utf8) {
            return fail(path_utf8.error());
        }
        if (path_utf8->empty()) {
            return fail(errc::malformed_data);
        }

        const auto instance_id =
            get_device_instance_id(dev_info.get(), &dev_data);
        if (!instance_id) {
            return fail(instance_id.error());
        }

        // A device can expose more than one interface. Deduplicate by its
        // device-instance identifier rather than by interface path.
        bool duplicate = false;
        for (const auto& existing : devices) {
            if (existing.id == *instance_id) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        ::syscape::camera::camera_device dev;
        dev.id = *instance_id;
        dev.device_path = *path_utf8;

        const auto friendly_name = get_device_property_string(
            dev_info.get(), &dev_data, SPDRP_FRIENDLYNAME);
        const auto device_desc = get_device_property_string(
            dev_info.get(), &dev_data, SPDRP_DEVICEDESC);
        if (!friendly_name || !device_desc) {
            return fail(!friendly_name ? friendly_name.error()
                                       : device_desc.error());
        }

        if (friendly_name->has_value() && !(**friendly_name).empty()) {
            dev.name = **friendly_name;
        } else if (device_desc->has_value() && !(**device_desc).empty()) {
            dev.name = **device_desc;
        } else {
            dev.name = *instance_id;
        }

        const auto driver_name =
            get_device_property_string(dev_info.get(), &dev_data, SPDRP_DRIVER);
        if (!driver_name) {
            return fail(driver_name.error());
        }
        if (driver_name->has_value() && !(**driver_name).empty()) {
            dev.driver = **driver_name;
        }

        const auto hwid = get_device_property_string(dev_info.get(), &dev_data,
                                                     SPDRP_HARDWAREID);
        if (!hwid) {
            return fail(hwid.error());
        }
        if (hwid->has_value()) {
            const auto parsed_id = parse_windows_hardware_id(**hwid);
            if (!parsed_id) {
                return fail(parsed_id.error());
            }
            if (parsed_id->has_value()) {
                dev.hardware_id = **parsed_id;
            }
            if (camera_common::contains_ignore_case(**hwid, "USB\\")) {
                dev.connection = ::syscape::camera::camera_connection::usb;
            } else if (camera_common::contains_ignore_case(**hwid, "PCI\\")) {
                dev.connection = ::syscape::camera::camera_connection::pci;
            }
        }

        // Membership in GUID_DEVINTERFACE_CAMERA establishes camera capture,
        // but SetupAPI does not establish the remaining stream capabilities.
        ::syscape::camera::camera_capabilities caps;
        caps.has_video_capture = true;
        dev.capabilities = caps;

        devices.push_back(std::move(dev));
    }

    return devices;
}

inline result<std::vector<::syscape::camera::camera_device>> devices() {
    return enumerate_windows_cameras();
}

inline result<std::size_t> device_count() {
    const auto res = devices();
    if (!res) {
        return fail(res.error());
    }
    return res->size();
}

inline result<std::vector<::syscape::camera::camera_device>> capture_devices() {
    const auto res = devices();
    if (!res) {
        return fail(res.error());
    }
    return camera_common::filter_capture_devices(*res);
}

inline result<::syscape::camera::camera_device> default_device() {
    return fail(errc::not_supported);
}

} // namespace camera_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_CAMERA_WINDOWS_HPP
