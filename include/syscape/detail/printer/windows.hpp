#ifndef SYSCAPE_DETAIL_PRINTER_WINDOWS_HPP
#define SYSCAPE_DETAIL_PRINTER_WINDOWS_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <winspool.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <syscape/detail/printer/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/error.hpp>
#include <syscape/printer.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace printer_backend {

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

inline result<std::wstring> copy_bounded_wide_string(
    const wchar_t* value,
    const std::vector<BYTE>& buffer) {
    if (value == nullptr) { return std::wstring{}; }
    const std::uintptr_t begin =
        reinterpret_cast<std::uintptr_t>(buffer.data());
    if (buffer.size() > (std::numeric_limits<std::uintptr_t>::max)() - begin) {
        return fail(errc::value_too_large);
    }
    const std::uintptr_t end = begin + buffer.size();
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(value);
    if (address < begin || address >= end ||
        address % alignof(wchar_t) != 0U) {
        return fail(errc::malformed_data);
    }

    const std::size_t offset = static_cast<std::size_t>(address - begin);
    const std::size_t maximum_units = (buffer.size() - offset) / sizeof(wchar_t);
    std::wstring copied;
    copied.reserve(maximum_units);
    for (std::size_t i = 0U; i < maximum_units; ++i) {
        wchar_t unit = L'\0';
        std::memcpy(&unit, buffer.data() + offset + i * sizeof(wchar_t),
                    sizeof(unit));
        if (unit == L'\0') { return copied; }
        copied.push_back(unit);
    }
    return fail(errc::malformed_data);
}

inline std::error_code windows_error(DWORD value) noexcept {
    if (value > static_cast<DWORD>((std::numeric_limits<int>::max)())) {
        return std::error_code(ERROR_INVALID_PARAMETER,
                               std::system_category());
    }
    return std::error_code(static_cast<int>(value), std::system_category());
}

inline printer::printer_state map_win32_printer_state(DWORD status) noexcept {
    if (status & (PRINTER_STATUS_ERROR | PRINTER_STATUS_PAUSED |
                  PRINTER_STATUS_PAPER_JAM | PRINTER_STATUS_PAPER_OUT |
                  PRINTER_STATUS_OFFLINE | PRINTER_STATUS_DOOR_OPEN |
                  PRINTER_STATUS_NO_TONER | PRINTER_STATUS_OUT_OF_MEMORY |
                  PRINTER_STATUS_USER_INTERVENTION | PRINTER_STATUS_NOT_AVAILABLE)) {
        return printer::printer_state::stopped;
    }
    if (status & (PRINTER_STATUS_PRINTING | PRINTER_STATUS_PROCESSING |
                  PRINTER_STATUS_BUSY | PRINTER_STATUS_INITIALIZING |
                  PRINTER_STATUS_WARMING_UP)) {
        return printer::printer_state::processing;
    }
    if (status == 0 || (status & (PRINTER_STATUS_WAITING | PRINTER_STATUS_POWER_SAVE))) {
        return printer::printer_state::idle;
    }
    return printer::printer_state::unknown;
}

inline printer::printer_type classify_win32_printer_type(
    DWORD attributes,
    std::string_view name,
    std::string_view driver,
    std::string_view port) noexcept {
    if ((attributes & PRINTER_ATTRIBUTE_FAX) != 0 ||
        printer_common::equals_ignore_case(port, "PORTPROMPT:") ||
        printer_common::equals_ignore_case(port, "nul:") ||
        printer_common::contains_ignore_case(name, "PDF") ||
        printer_common::contains_ignore_case(name, "XPS") ||
        printer_common::contains_ignore_case(name, "Fax") ||
        printer_common::contains_ignore_case(name, "Document Writer") ||
        printer_common::contains_ignore_case(name, "OneNote") ||
        printer_common::contains_ignore_case(driver, "PDF") ||
        printer_common::contains_ignore_case(driver, "XPS") ||
        printer_common::contains_ignore_case(driver, "Fax")) {
        return printer::printer_type::virtual_printer;
    }
    if ((attributes & PRINTER_ATTRIBUTE_NETWORK) != 0 ||
        printer_common::starts_with_ignore_case(port, "IP_") ||
        printer_common::starts_with_ignore_case(port, "WSD-") ||
        printer_common::starts_with_ignore_case(port, "http://") ||
        printer_common::starts_with_ignore_case(port, "https://")) {
        return printer::printer_type::network;
    }
    if ((attributes & PRINTER_ATTRIBUTE_LOCAL) != 0 ||
        printer_common::starts_with_ignore_case(port, "USB") ||
        printer_common::starts_with_ignore_case(port, "LPT") ||
        printer_common::starts_with_ignore_case(port, "COM")) {
        return printer::printer_type::local;
    }
    return printer::printer_type::unknown;
}

inline result<printer::printer_capabilities> query_win32_capabilities(
    const wchar_t* printer_name,
    const wchar_t* port_name) {
    printer::printer_capabilities caps;
    constexpr std::size_t maximum_capability_values = 4096U;

    // Check color support
    const LONG color_res = ::DeviceCapabilitiesW(
        printer_name, port_name, DC_COLORDEVICE, nullptr, nullptr);
    if (color_res >= 0) {
        caps.color = (color_res == 1);
    }

    // Check duplex support
    const LONG duplex_res = ::DeviceCapabilitiesW(
        printer_name, port_name, DC_DUPLEX, nullptr, nullptr);
    if (duplex_res >= 0) {
        caps.duplex = (duplex_res == 1);
    }

    const LONG collate_res = ::DeviceCapabilitiesW(
        printer_name, port_name, DC_COLLATE, nullptr, nullptr);
    if (collate_res >= 0) {
        caps.collate = (collate_res == 1);
    }

    // Check max copies
    const LONG copies_res = ::DeviceCapabilitiesW(
        printer_name, port_name, DC_COPIES, nullptr, nullptr);
    if (copies_res > 0) {
        caps.max_copies = static_cast<std::uint32_t>(copies_res);
        caps.copies = (copies_res > 1);
    }

    // Check supported paper sizes
    const LONG paper_count = ::DeviceCapabilitiesW(
        printer_name, port_name, DC_PAPERNAMES, nullptr, nullptr);
    if (paper_count > static_cast<LONG>(maximum_capability_values)) {
        return fail(errc::value_too_large);
    }
    if (paper_count > 0) {
        // DeviceCapabilitiesW has no buffer-size parameter. Reserve the full
        // accepted maximum so a normal count increase between calls cannot
        // overrun a buffer allocated from the stale sizing result.
        std::vector<wchar_t> paper_names_buf(maximum_capability_values * 64U);
        const LONG fetched = ::DeviceCapabilitiesW(
            printer_name, port_name, DC_PAPERNAMES, paper_names_buf.data(), nullptr);
        if (fetched < 0) {
            return fail(errc::temporarily_unavailable);
        }
        if (fetched > static_cast<LONG>(maximum_capability_values)) {
            return fail(errc::value_too_large);
        }
        if (fetched > 0) {
            const auto count = static_cast<std::size_t>(fetched);
            for (std::size_t i = 0; i < count; ++i) {
                const wchar_t* const name_ptr = paper_names_buf.data() + (i * 64U);
                std::size_t len = 0;
                while (len < 64U && name_ptr[len] != L'\0') {
                    ++len;
                }
                if (len == 64U) { return fail(errc::malformed_data); }
                const auto name_utf8 = wide_to_utf8(std::wstring_view(name_ptr, len));
                if (!name_utf8) { return fail(name_utf8.error()); }
                if (!name_utf8->empty()) {
                    caps.supported_media.push_back(*name_utf8);
                }
            }
        }
    }

    const LONG resolution_count = ::DeviceCapabilitiesW(
        printer_name, port_name, DC_ENUMRESOLUTIONS, nullptr, nullptr);
    if (resolution_count > static_cast<LONG>(maximum_capability_values)) {
        return fail(errc::value_too_large);
    }
    if (resolution_count > 0) {
        std::vector<LONG> resolutions(maximum_capability_values * 2U);
        const LONG fetched = ::DeviceCapabilitiesW(
            printer_name, port_name, DC_ENUMRESOLUTIONS,
            reinterpret_cast<wchar_t*>(resolutions.data()), nullptr);
        if (fetched < 0) {
            return fail(errc::temporarily_unavailable);
        }
        if (fetched > static_cast<LONG>(maximum_capability_values)) {
            return fail(errc::value_too_large);
        }
        const auto count = static_cast<std::size_t>(fetched);
        for (std::size_t i = 0; i < count; ++i) {
            const LONG x = resolutions[i * 2U];
            const LONG y = resolutions[i * 2U + 1U];
            if (x <= 0 || y <= 0) { return fail(errc::malformed_data); }
            caps.supported_resolutions.push_back(
                std::to_string(x) + "x" + std::to_string(y) + " dpi");
        }
    }

    return caps;
}

inline result<std::string> get_default_printer_name() {
    DWORD size = 0;
    const BOOL sizing_succeeded = ::GetDefaultPrinterW(nullptr, &size);
    if (sizing_succeeded) { return fail(errc::malformed_data); }
    const DWORD sizing_error = ::GetLastError();
    if (sizing_error == ERROR_FILE_NOT_FOUND) {
        return fail(errc::not_found);
    }
    if (sizing_error != ERROR_INSUFFICIENT_BUFFER) {
        return fail(windows_error(sizing_error));
    }
    if (size == 0) { return fail(errc::malformed_data); }
    if (size > 32768U) { return fail(errc::value_too_large); }
    std::vector<wchar_t> buf(size);
    if (!::GetDefaultPrinterW(buf.data(), &size)) {
        const DWORD error = ::GetLastError();
        if (error == ERROR_FILE_NOT_FOUND ||
            error == ERROR_INSUFFICIENT_BUFFER) {
            return fail(errc::temporarily_unavailable);
        }
        return fail(windows_error(error));
    }
    if (size == 0 || size > buf.size() || buf[size - 1U] != L'\0') {
        return fail(errc::malformed_data);
    }
    --size;
    if (size == 0) { return fail(errc::malformed_data); }
    return wide_to_utf8(std::wstring_view(buf.data(), size));
}

inline result<std::vector<printer::printer_info>> printers() {
    DWORD bytes_needed = 0;
    DWORD count = 0;
    const DWORD flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;

    const BOOL sizing_succeeded =
        ::EnumPrintersW(flags, nullptr, 2, nullptr, 0, &bytes_needed, &count);
    if (sizing_succeeded) {
        if (bytes_needed == 0 && count == 0) {
            return std::vector<printer::printer_info>{};
        }
        return fail(errc::malformed_data);
    }
    const DWORD sizing_error = ::GetLastError();
    if (sizing_error != ERROR_INSUFFICIENT_BUFFER || bytes_needed == 0) {
        return fail(windows_error(sizing_error));
    }
    if (bytes_needed > 64U * 1024U * 1024U) {
        return fail(errc::value_too_large);
    }

    std::vector<BYTE> buffer(bytes_needed);
    if (!::EnumPrintersW(flags, nullptr, 2, buffer.data(), bytes_needed,
                         &bytes_needed, &count)) {
        const DWORD error = ::GetLastError();
        if (error == ERROR_INSUFFICIENT_BUFFER) {
            return fail(errc::temporarily_unavailable);
        }
        return fail(windows_error(error));
    }
    if (count > bytes_needed / sizeof(PRINTER_INFO_2W)) {
        return fail(errc::malformed_data);
    }
    if (bytes_needed > buffer.size()) { return fail(errc::malformed_data); }

    std::string default_name;
    const auto def_res = get_default_printer_name();
    if (def_res) {
        default_name = *def_res;
    } else if (def_res.error() != errc::not_found) {
        return fail(def_res.error());
    }

    std::vector<printer::printer_info> result_list;
    result_list.reserve(count);

    for (DWORD i = 0; i < count; ++i) {
        PRINTER_INFO_2W item{};
        std::memcpy(&item, buffer.data() +
                               static_cast<std::size_t>(i) * sizeof(item),
                    sizeof(item));
        printer::printer_info p;

        if (!item.pPrinterName) { return fail(errc::malformed_data); }
        const auto native_name = copy_bounded_wide_string(item.pPrinterName, buffer);
        if (!native_name) { return fail(native_name.error()); }
        const auto name_utf8 = wide_to_utf8(*native_name);
        if (!name_utf8) { return fail(name_utf8.error()); }
        if (name_utf8->empty()) { return fail(errc::malformed_data); }
        p.id = *name_utf8;
        p.name = *name_utf8;
        if (item.pDriverName) {
            const auto native_driver =
                copy_bounded_wide_string(item.pDriverName, buffer);
            if (!native_driver) { return fail(native_driver.error()); }
            const auto driver_utf8 = wide_to_utf8(*native_driver);
            if (!driver_utf8) { return fail(driver_utf8.error()); }
            p.driver_name = *driver_utf8;
        }
        if (item.pLocation) {
            const auto native_location =
                copy_bounded_wide_string(item.pLocation, buffer);
            if (!native_location) { return fail(native_location.error()); }
            const auto loc_utf8 = wide_to_utf8(*native_location);
            if (!loc_utf8) { return fail(loc_utf8.error()); }
            p.location = *loc_utf8;
        }
        if (item.pComment) {
            const auto native_comment =
                copy_bounded_wide_string(item.pComment, buffer);
            if (!native_comment) { return fail(native_comment.error()); }
            const auto comment_utf8 = wide_to_utf8(*native_comment);
            if (!comment_utf8) { return fail(comment_utf8.error()); }
            p.description = *comment_utf8;
        }
        std::wstring native_port;
        if (item.pPortName) {
            const auto port = copy_bounded_wide_string(item.pPortName, buffer);
            if (!port) { return fail(port.error()); }
            native_port = *port;
            const auto port_utf8 = wide_to_utf8(native_port);
            if (!port_utf8) { return fail(port_utf8.error()); }
            p.uri = *port_utf8;
        }

        p.type = classify_win32_printer_type(
            item.Attributes, p.name, p.driver_name, p.uri);
        p.state = map_win32_printer_state(item.Status);
        p.is_default = !default_name.empty() && p.name == default_name;
        p.is_shared = (item.Attributes & PRINTER_ATTRIBUTE_SHARED) != 0;
        p.queued_job_count = item.cJobs;

        const auto capabilities = query_win32_capabilities(
            native_name->c_str(), item.pPortName ? native_port.c_str() : nullptr);
        if (!capabilities) { return fail(capabilities.error()); }
        p.capabilities = *capabilities;

        result_list.push_back(std::move(p));
    }

    std::sort(result_list.begin(), result_list.end(),
              [](const auto& a, const auto& b) {
                  return printer_common::natural_less(a.name, b.name);
              });

    if (!default_name.empty() &&
        std::none_of(result_list.begin(), result_list.end(),
                     [](const printer::printer_info& item) {
                         return item.is_default.value_or(false);
                     })) {
        return fail(errc::temporarily_unavailable);
    }

    return result_list;
}

inline result<std::size_t> printer_count() {
    const auto list = printers();
    if (!list) {
        return fail(list.error());
    }
    return list->size();
}

inline result<printer::printer_info> default_printer() {
    const auto list = printers();
    if (!list) {
        return fail(list.error());
    }
    if (list->empty()) {
        return fail(errc::not_found);
    }

    return printer_common::marked_default_printer(*list);
}

inline result<printer::printer_info> find_printer(std::string_view name_or_id) {
    const auto list = printers();
    if (!list) {
        return fail(list.error());
    }

    for (const auto& item : *list) {
        if (item.id == name_or_id || item.name == name_or_id) {
            return item;
        }
    }

    for (const auto& item : *list) {
        if (printer_common::equals_ignore_case(item.id, name_or_id) ||
            printer_common::equals_ignore_case(item.name, name_or_id)) {
            return item;
        }
    }

    return fail(errc::not_found);
}

} // namespace printer_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_PRINTER_WINDOWS_HPP
