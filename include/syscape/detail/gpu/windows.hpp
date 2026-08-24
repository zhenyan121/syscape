#ifndef SYSCAPE_DETAIL_GPU_WINDOWS_HPP
#define SYSCAPE_DETAIL_GPU_WINDOWS_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <windows.h>

#include <syscape/detail/gpu/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/gpu.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace gpu_backend {

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

/// Parses a 4-hex-digit ID following a prefix like "VEN_" or "DEV_".
/// Returns malformed_data if the prefix is present but contains invalid or truncated hex.
inline result<std::optional<std::uint32_t>> parse_pnp_hex_field(
    std::string_view pnp_id, std::string_view prefix) noexcept {
    const std::size_t pos = pnp_id.find(prefix);
    if (pos == std::string_view::npos) {
        return std::optional<std::uint32_t>(std::nullopt);
    }
    const std::size_t start = pos + prefix.size();
    if (start + 4U > pnp_id.size()) {
        return fail(errc::malformed_data);
    }
    const std::size_t end = start + 4U;
    if (end < pnp_id.size() && pnp_id[end] != '&') {
        return fail(errc::malformed_data);
    }
    const auto parsed = gpu_common::parse_hex(pnp_id.substr(start, 4U));
    if (!parsed) {
        return fail(errc::malformed_data);
    }
    return std::optional<std::uint32_t>(*parsed);
}

inline result<std::vector<::syscape::gpu::gpu_device>> collect_devices() {
    std::vector<::syscape::gpu::gpu_device> list;
    DWORD dev_index = 0U;

    for (;;) {
        DISPLAY_DEVICEW dd;
        dd.cb = sizeof(dd);
        if (!::EnumDisplayDevicesW(nullptr, dev_index, &dd, 0)) {
            break;
        }
        ++dev_index;

        // Skip pseudo/mirroring drivers
        if ((dd.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) != 0) {
            continue;
        }

        const auto name_utf8 = wide_to_utf8(dd.DeviceString);
        if (!name_utf8) {
            return fail(name_utf8.error());
        }

        const auto id_utf8 = wide_to_utf8(dd.DeviceName);
        if (!id_utf8) {
            return fail(id_utf8.error());
        }

        const auto pnp_utf8 = wide_to_utf8(dd.DeviceID);
        if (!pnp_utf8) {
            return fail(pnp_utf8.error());
        }

        ::syscape::gpu::gpu_device dev;
        dev.id = *id_utf8;

        if (!name_utf8->empty()) {
            dev.name = *name_utf8;
        }

        if (!pnp_utf8->empty()) {
            const auto ven_res = parse_pnp_hex_field(*pnp_utf8, "VEN_");
            if (!ven_res) { return fail(ven_res.error()); }
            dev.vendor_id = *ven_res;

            const auto dev_res = parse_pnp_hex_field(*pnp_utf8, "DEV_");
            if (!dev_res) { return fail(dev_res.error()); }
            dev.device_id = *dev_res;
        }

        if (dev.vendor_id.has_value()) {
            dev.vendor = gpu_common::classify_pci_vendor_id(*dev.vendor_id);
        } else if (dev.name.has_value() && !dev.name->empty()) {
            dev.vendor = gpu_common::classify_vendor_name(*dev.name);
        } else {
            dev.vendor = ::syscape::gpu::gpu_vendor::unknown;
        }

        dev.vendor_name = gpu_common::vendor_to_string(dev.vendor);
        dev.is_primary = (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0;

        list.push_back(std::move(dev));
    }

    return list;
}

inline result<std::vector<::syscape::gpu::gpu_device>> devices() {
    return collect_devices();
}

inline result<std::size_t> device_count() {
    const auto res = collect_devices();
    if (!res) { return fail(res.error()); }
    return res->size();
}

inline result<::syscape::gpu::gpu_device> primary_device() {
    const auto res = collect_devices();
    if (!res) { return fail(res.error()); }
    if (res->empty()) { return fail(errc::not_found); }

    for (const auto& dev : *res) {
        if (dev.is_primary.has_value() && *dev.is_primary) {
            return dev;
        }
    }
    return fail(errc::not_found);
}

} // namespace gpu_backend
} // namespace detail
} // namespace syscape

#endif
