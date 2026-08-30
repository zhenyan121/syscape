#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include <windows.h>

#include <syscape/gpu.hpp>
#include <syscape/detail/gpu/common.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_windows_pnp_parsing() {
    namespace backend = syscape::detail::gpu_backend;
    namespace common = syscape::detail::gpu_common;

    // Valid PNP ID
    const std::string_view pnp_nvidia = "PCI\\VEN_10DE&DEV_2D59&SUBSYS_123410DE&REV_A1";
    const auto ven = backend::parse_pnp_hex_field(pnp_nvidia, "VEN_");
    const auto dev = backend::parse_pnp_hex_field(pnp_nvidia, "DEV_");

    expect(ven && ven->has_value() && **ven == 0x10deU, "VEN_10DE must parse to 0x10de");
    expect(dev && dev->has_value() && **dev == 0x2d59U, "DEV_2D59 must parse to 0x2d59");
    expect(common::classify_pci_vendor_id(**ven) == syscape::gpu::gpu_vendor::nvidia,
           "0x10de must classify as NVIDIA");

    // Absent prefix
    const std::string_view pnp_invalid = "PCI\\UNKNOWN_DEVICE";
    const auto ven_absent = backend::parse_pnp_hex_field(pnp_invalid, "VEN_");
    expect(ven_absent && !ven_absent->has_value(),
           "Missing VEN_ must return nullopt");

    // Malformed hex in VEN_
    const std::string_view pnp_malformed = "PCI\\VEN_10GG&DEV_2D59";
    const auto ven_malformed = backend::parse_pnp_hex_field(pnp_malformed, "VEN_");
    expect(!ven_malformed && ven_malformed.error() == syscape::errc::malformed_data,
           "Malformed VEN_ hex must return malformed_data");

    // Truncated VEN_
    const std::string_view pnp_truncated = "PCI\\VEN_10";
    const auto ven_truncated = backend::parse_pnp_hex_field(pnp_truncated, "VEN_");
    expect(!ven_truncated && ven_truncated.error() == syscape::errc::malformed_data,
           "Truncated VEN_ must return malformed_data");

    // More than four hexadecimal digits
    const std::string_view pnp_overlong = "PCI\\VEN_10DE0&DEV_2D59";
    const auto ven_overlong = backend::parse_pnp_hex_field(pnp_overlong, "VEN_");
    expect(!ven_overlong && ven_overlong.error() == syscape::errc::malformed_data,
           "Overlong VEN_ must return malformed_data");
}

void test_windows_utf8_encoding() {
    namespace backend = syscape::detail::gpu_backend;

    // Valid wide string
    const std::wstring valid_wide = L"NVIDIA GeForce RTX 4070";
    const auto valid_res = backend::wide_to_utf8(valid_wide);
    expect(valid_res.has_value() && *valid_res == "NVIDIA GeForce RTX 4070",
           "Valid wide string must convert to UTF-8");

    // Invalid surrogate
    const wchar_t invalid_wide[] = { static_cast<wchar_t>(0xD800), 0 };
    const auto invalid_res = backend::wide_to_utf8(invalid_wide);
    expect(!invalid_res && invalid_res.error() == syscape::errc::invalid_encoding,
           "Unpaired surrogate must report invalid_encoding");
}

void test_windows_live_query() {
    const auto devs = syscape::gpu::devices();
    expect(devs.has_value(), "devices() query must return a result");
    const auto count = syscape::gpu::device_count();
    expect(count.has_value() && *count == devs->size(),
           "device_count() must match devices() size");
}

} // namespace

int main() {
    test_windows_pnp_parsing();
    test_windows_utf8_encoding();
    test_windows_live_query();
    return failures == 0 ? 0 : 1;
}
