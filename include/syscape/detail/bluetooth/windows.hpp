#ifndef SYSCAPE_DETAIL_BLUETOOTH_WINDOWS_HPP
#define SYSCAPE_DETAIL_BLUETOOTH_WINDOWS_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <bthsdpdef.h>
#include <bluetoothapis.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <syscape/bluetooth.hpp>
#include <syscape/detail/bluetooth/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace bluetooth_backend {

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

inline std::size_t bounded_wide_length(
    const wchar_t* value, std::size_t maximum_size) noexcept {
    std::size_t length = 0U;
    while (length < maximum_size && value[length] != L'\0') {
        ++length;
    }
    return length;
}

inline std::error_code windows_error(DWORD value) noexcept {
    if (value > static_cast<DWORD>((std::numeric_limits<int>::max)())) {
        return make_error_code(errc::io_error);
    }
    return std::error_code(static_cast<int>(value), std::system_category());
}

inline result<std::vector<bluetooth::adapter_info>> adapters() {
    std::vector<bluetooth::adapter_info> result_list;

    BLUETOOTH_FIND_RADIO_PARAMS params{};
    params.dwSize = sizeof(BLUETOOTH_FIND_RADIO_PARAMS);

    HANDLE hRadio = nullptr;
    HBLUETOOTH_RADIO_FIND hFind = ::BluetoothFindFirstRadio(&params, &hRadio);
    if (hFind == nullptr) {
        const DWORD err = ::GetLastError();
        if (err == ERROR_NO_MORE_ITEMS || err == ERROR_NOT_FOUND ||
            err == ERROR_FILE_NOT_FOUND) {
            return result_list;
        }
        if (err == ERROR_ACCESS_DENIED) {
            return fail(errc::permission_denied);
        }
        return fail(windows_error(err));
    }

    std::size_t index = 0;
    do {
        if (hRadio != nullptr) {
            BLUETOOTH_RADIO_INFO rinfo{};
            rinfo.dwSize = sizeof(BLUETOOTH_RADIO_INFO);
            const DWORD info_error = ::BluetoothGetRadioInfo(hRadio, &rinfo);
            if (info_error == ERROR_SUCCESS) {
                bluetooth::adapter_info info;
                info.id = "radio" + std::to_string(index);

                auto name_utf8 = wide_to_utf8(std::wstring_view(
                    rinfo.szName,
                    bounded_wide_length(
                        rinfo.szName,
                        sizeof(rinfo.szName) / sizeof(rinfo.szName[0]))));
                if (!name_utf8) {
                    ::CloseHandle(hRadio);
                    ::BluetoothFindRadioClose(hFind);
                    return fail(name_utf8.error());
                }
                info.name = name_utf8->empty() ? info.id : *name_utf8;

                info.address = bluetooth_common::format_mac_bytes(rinfo.address.rgBytes, true);
                info.power_state = bluetooth::adapter_power_state::on;
                info.is_connectable = (::BluetoothIsConnectable(hRadio) == TRUE);
                info.is_discoverable = (::BluetoothIsDiscoverable(hRadio) == TRUE);
                info.manufacturer_id = static_cast<std::uint16_t>(rinfo.manufacturer);
                info.lmp_subversion = static_cast<std::uint16_t>(rinfo.lmpSubversion);

                result_list.push_back(std::move(info));
                ++index;
            } else {
                ::CloseHandle(hRadio);
                ::BluetoothFindRadioClose(hFind);
                if (info_error == ERROR_ACCESS_DENIED) {
                    return fail(errc::permission_denied);
                }
                return fail(windows_error(info_error));
            }
            ::CloseHandle(hRadio);
            hRadio = nullptr;
        }
    } while (::BluetoothFindNextRadio(hFind, &hRadio));

    const DWORD enumeration_error = ::GetLastError();
    ::BluetoothFindRadioClose(hFind);
    if (enumeration_error != ERROR_NO_MORE_ITEMS) {
        if (enumeration_error == ERROR_ACCESS_DENIED) {
            return fail(errc::permission_denied);
        }
        return fail(windows_error(enumeration_error));
    }
    return result_list;
}

inline result<std::size_t> adapter_count() {
    auto res = adapters();
    if (!res) {
        return fail(res.error());
    }
    return res->size();
}

inline result<bluetooth::adapter_info> default_adapter() {
    auto res = adapters();
    if (!res) {
        return fail(res.error());
    }
    if (res->empty()) {
        return fail(errc::not_found);
    }
    return (*res)[0];
}

inline result<std::vector<bluetooth::device_info>>
enumerate_devices(bool include_unknown) {
    std::vector<bluetooth::device_info> result_list;

    BLUETOOTH_DEVICE_SEARCH_PARAMS search_params{};
    search_params.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS);
    search_params.fReturnAuthenticated = TRUE;
    search_params.fReturnRemembered = TRUE;
    search_params.fReturnUnknown = include_unknown ? TRUE : FALSE;
    search_params.fReturnConnected = TRUE;
    search_params.fIssueInquiry = FALSE; // Non-invasive, no active RF discovery
    search_params.cTimeoutMultiplier = 0;
    search_params.hRadio = nullptr; // Search across all local radios

    BLUETOOTH_DEVICE_INFO dev_info{};
    dev_info.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);

    HBLUETOOTH_DEVICE_FIND hFind = ::BluetoothFindFirstDevice(&search_params, &dev_info);
    if (hFind == nullptr) {
        const DWORD err = ::GetLastError();
        if (err == ERROR_NO_MORE_ITEMS || err == ERROR_NOT_FOUND ||
            err == ERROR_FILE_NOT_FOUND) {
            return result_list;
        }
        if (err == ERROR_ACCESS_DENIED) {
            return fail(errc::permission_denied);
        }
        return fail(windows_error(err));
    }

    do {
        bluetooth::device_info dev;
        dev.address = bluetooth_common::format_mac_bytes(dev_info.Address.rgBytes, true);
        auto name_utf8 = wide_to_utf8(std::wstring_view(
            dev_info.szName,
            bounded_wide_length(
                dev_info.szName,
                sizeof(dev_info.szName) / sizeof(dev_info.szName[0]))));
        if (!name_utf8) {
            ::BluetoothFindDeviceClose(hFind);
            return fail(name_utf8.error());
        }
        if (!name_utf8->empty()) {
            dev.name = *name_utf8;
        }
        dev.is_connected = (dev_info.fConnected == TRUE);
        dev.is_paired = (dev_info.fRemembered == TRUE || dev_info.fAuthenticated == TRUE);
        dev.class_of_device = dev_info.ulClassofDevice;
        dev.device_type = bluetooth_common::decode_major_device_class(dev_info.ulClassofDevice);

        result_list.push_back(std::move(dev));
    } while (::BluetoothFindNextDevice(hFind, &dev_info));

    const DWORD enumeration_error = ::GetLastError();
    ::BluetoothFindDeviceClose(hFind);
    if (enumeration_error != ERROR_NO_MORE_ITEMS) {
        if (enumeration_error == ERROR_ACCESS_DENIED) {
            return fail(errc::permission_denied);
        }
        return fail(windows_error(enumeration_error));
    }
    return result_list;
}

inline result<std::vector<bluetooth::device_info>> paired_devices() {
    auto res = enumerate_devices(false);
    if (!res) {
        return fail(res.error());
    }
    std::vector<bluetooth::device_info> paired;
    for (auto& dev : *res) {
        if (dev.is_paired == true) {
            paired.push_back(std::move(dev));
        }
    }
    return paired;
}

inline result<std::vector<bluetooth::device_info>> connected_devices() {
    auto res = enumerate_devices(true);
    if (!res) {
        return fail(res.error());
    }
    std::vector<bluetooth::device_info> connected;
    for (auto& dev : *res) {
        if (dev.is_connected == true) {
            connected.push_back(std::move(dev));
        }
    }
    return connected;
}

} // namespace bluetooth_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_BLUETOOTH_WINDOWS_HPP
