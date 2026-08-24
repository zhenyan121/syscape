#ifndef SYSCAPE_DETAIL_INPUT_WINDOWS_HPP
#define SYSCAPE_DETAIL_INPUT_WINDOWS_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <syscape/input.hpp>
#include <syscape/detail/input/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace input_backend {

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

inline ::syscape::input::bus_type bus_from_device_path(std::string_view path) noexcept {
    if (input_common::contains_ignore_case(path, "BTH#") ||
        input_common::contains_ignore_case(path, "BTHENUM#") ||
        input_common::contains_ignore_case(path, "BTHLEDEVICE#")) {
        return ::syscape::input::bus_type::bluetooth;
    }
    if (input_common::contains_ignore_case(path, "USB#")) {
        return ::syscape::input::bus_type::usb;
    }
    if (input_common::contains_ignore_case(path, "ACPI#") ||
        input_common::contains_ignore_case(path, "ROOT#")) {
        return ::syscape::input::bus_type::virtual_bus;
    }
    return ::syscape::input::bus_type::unknown;
}

inline result<std::vector<::syscape::input::input_device>> enumerate_raw_input_devices() {
    UINT num_devices = 0U;
    if (::GetRawInputDeviceList(
            nullptr, &num_devices, sizeof(RAWINPUTDEVICELIST)) ==
        static_cast<UINT>(-1)) {
        return fail(std::error_code(
            static_cast<int>(::GetLastError()), std::system_category()));
    }

    if (num_devices == 0U) {
        return std::vector<::syscape::input::input_device>{};
    }

    std::vector<RAWINPUTDEVICELIST> raw_list;
    UINT retrieved = static_cast<UINT>(-1);
    for (unsigned int attempt = 0U; attempt < 4U; ++attempt) {
        raw_list.resize(num_devices);
        retrieved = ::GetRawInputDeviceList(
            raw_list.data(), &num_devices, sizeof(RAWINPUTDEVICELIST));
        if (retrieved != static_cast<UINT>(-1)) {
            break;
        }
        const DWORD error = ::GetLastError();
        if (error != ERROR_INSUFFICIENT_BUFFER) {
            return fail(std::error_code(static_cast<int>(error), std::system_category()));
        }
    }
    if (retrieved == static_cast<UINT>(-1)) {
        return fail(errc::temporarily_unavailable);
    }
    raw_list.resize(retrieved);

    std::vector<::syscape::input::input_device> devices;
    devices.reserve(raw_list.size());

    for (const auto& raw_dev : raw_list) {
        ::syscape::input::input_device dev;

        // Query device name length
        UINT name_size = 0U;
        if (::GetRawInputDeviceInfoW(
                raw_dev.hDevice, RIDI_DEVICENAME, nullptr, &name_size) ==
            static_cast<UINT>(-1)) {
            return fail(std::error_code(
                static_cast<int>(::GetLastError()), std::system_category()));
        }

        if (name_size > 0U) {
            std::vector<wchar_t> name_buf;
            UINT copied = static_cast<UINT>(-1);
            for (unsigned int attempt = 0U; attempt < 4U; ++attempt) {
                name_buf.resize(name_size);
                copied = ::GetRawInputDeviceInfoW(
                    raw_dev.hDevice, RIDI_DEVICENAME,
                    name_buf.data(), &name_size);
                if (copied != static_cast<UINT>(-1)) {
                    break;
                }
                const DWORD error = ::GetLastError();
                if (error != ERROR_INSUFFICIENT_BUFFER) {
                    return fail(std::error_code(
                        static_cast<int>(error), std::system_category()));
                }
            }
            if (copied == static_cast<UINT>(-1)) {
                return fail(errc::temporarily_unavailable);
            }
            std::size_t actual_length = 0U;
            while (actual_length < name_buf.size() &&
                   name_buf[actual_length] != L'\0') {
                ++actual_length;
            }
            const auto converted = wide_to_utf8(
                std::wstring_view(name_buf.data(), actual_length));
            if (!converted) {
                return fail(converted.error());
            }
            dev.id = *converted;
        }

        dev.bus = bus_from_device_path(dev.id);

        // Query device info
        RID_DEVICE_INFO info{};
        info.cbSize = sizeof(RID_DEVICE_INFO);
        UINT info_size = sizeof(RID_DEVICE_INFO);
        if (::GetRawInputDeviceInfoW(
                raw_dev.hDevice, RIDI_DEVICEINFO, &info, &info_size) ==
            static_cast<UINT>(-1)) {
            return fail(std::error_code(
                static_cast<int>(::GetLastError()), std::system_category()));
        }
        {
            ::syscape::input::input_device_id hw_id;
            hw_id.bus = dev.bus;

            if (raw_dev.dwType == RIM_TYPEKEYBOARD) {
                dev.type = ::syscape::input::device_type::keyboard;
                dev.name = "Keyboard";
            } else if (raw_dev.dwType == RIM_TYPEMOUSE) {
                dev.type = ::syscape::input::device_type::mouse;
                dev.name = "Mouse";
            } else if (raw_dev.dwType == RIM_TYPEHID) {
                const DWORD maximum_id =
                    static_cast<DWORD>((std::numeric_limits<std::uint16_t>::max)());
                if (info.hid.dwVendorId > maximum_id ||
                    info.hid.dwProductId > maximum_id ||
                    info.hid.dwVersionNumber > maximum_id) {
                    return fail(errc::malformed_data);
                }
                hw_id.vendor_id = static_cast<std::uint16_t>(info.hid.dwVendorId);
                hw_id.product_id = static_cast<std::uint16_t>(info.hid.dwProductId);
                hw_id.version = static_cast<std::uint16_t>(info.hid.dwVersionNumber);

                // Generic Desktop Page (0x01)
                if (info.hid.usUsagePage == 0x01U) {
                    if (info.hid.usUsage == 0x04U) {
                        dev.type = ::syscape::input::device_type::joystick;
                        dev.name = "Joystick";
                    } else if (info.hid.usUsage == 0x05U) {
                        dev.type = ::syscape::input::device_type::gamepad;
                        dev.name = "Gamepad";
                    } else if (info.hid.usUsage == 0x06U) {
                        dev.type = ::syscape::input::device_type::keyboard;
                        dev.name = "HID Keyboard";
                    } else if (info.hid.usUsage == 0x02U) {
                        dev.type = ::syscape::input::device_type::mouse;
                        dev.name = "HID Mouse";
                    }
                } else if (info.hid.usUsagePage == 0x0DU) { // Digitizer Page
                    if (info.hid.usUsage == 0x01U || info.hid.usUsage == 0x02U) {
                        dev.type = ::syscape::input::device_type::drawing_tablet;
                        dev.name = "Digitizer Tablet";
                    } else if (info.hid.usUsage == 0x04U) {
                        dev.type = ::syscape::input::device_type::touchscreen;
                        dev.name = "Touch Screen";
                    } else if (info.hid.usUsage == 0x05U) {
                        dev.type = ::syscape::input::device_type::touchpad;
                        dev.name = "Touchpad";
                    }
                }

                dev.hardware_id = hw_id;
            }
        }

        if (dev.name.empty()) {
            dev.name = "Input Device";
        }

        devices.push_back(std::move(dev));
    }

    return devices;
}

inline result<std::vector<::syscape::input::input_device>> devices() {
    return enumerate_raw_input_devices();
}

inline result<std::vector<::syscape::input::input_device>> keyboards() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    return input_common::filter_by_type(*all, ::syscape::input::device_type::keyboard);
}

inline result<std::vector<::syscape::input::input_device>> mice() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    return input_common::filter_by_type(*all, ::syscape::input::device_type::mouse);
}

inline result<std::vector<::syscape::input::input_device>> touch_devices() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    return input_common::filter_touch_devices(*all);
}

inline result<std::vector<::syscape::input::input_device>> gamepads() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    return input_common::filter_gamepads(*all);
}

inline result<std::size_t> device_count() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    return all->size();
}

} // namespace input_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_INPUT_WINDOWS_HPP
