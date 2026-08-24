#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <syscape/gpu.hpp>
#include <syscape/detail/gpu/common.hpp>
#include <syscape/detail/gpu/linux.hpp>
#include <syscape/detail/utf8.hpp>

namespace {

void test_synthetic_hex_parsing() {
    using namespace syscape::detail::gpu_common;

    const auto h1 = parse_hex("0x10de");
    assert(h1 && *h1 == 0x10deU);

    const auto h2 = parse_hex("0X1002");
    assert(h2 && *h2 == 0x1002U);

    const auto h3 = parse_hex("8086");
    assert(h3 && *h3 == 0x8086U);

    const auto h4 = parse_hex(" 0x030000 \n");
    assert(h4 && *h4 == 0x030000U);

    const auto h5 = parse_hex("0");
    assert(h5 && *h5 == 0U);

    const auto h_inv = parse_hex("0xG123");
    assert(!h_inv && h_inv.error() == syscape::errc::malformed_data);

    const auto h_empty = parse_hex("   ");
    assert(!h_empty && h_empty.error() == syscape::errc::malformed_data);

    const auto h_overflow = parse_hex("0x100000000");
    assert(!h_overflow && h_overflow.error() == syscape::errc::value_too_large);
}

void test_synthetic_decimal_u64_parsing() {
    using syscape::detail::gpu_backend::parse_decimal_u64;

    const auto d1 = parse_decimal_u64("536870912");
    assert(d1 && *d1 == 536870912ULL);

    const auto d2 = parse_decimal_u64("0");
    assert(d2 && *d2 == 0ULL);

    const auto d3 = parse_decimal_u64(" 18446744073709551615 \n");
    assert(d3 && *d3 == 18446744073709551615ULL);

    const auto d_overflow = parse_decimal_u64("18446744073709551616");
    assert(!d_overflow && d_overflow.error() == syscape::errc::value_too_large);

    const auto d_inv = parse_decimal_u64("123abc456");
    assert(!d_inv && d_inv.error() == syscape::errc::malformed_data);

    const auto d_empty = parse_decimal_u64("   ");
    assert(!d_empty && d_empty.error() == syscape::errc::malformed_data);

    const auto d_neg = parse_decimal_u64("-500");
    assert(!d_neg && d_neg.error() == syscape::errc::malformed_data);
}

void test_synthetic_vendor_classification() {
    using namespace syscape::detail::gpu_common;
    using syscape::detail::gpu_backend::classify_driver_or_node;
    using syscape::gpu::gpu_vendor;

    assert(classify_pci_vendor_id(0x10deU) == gpu_vendor::nvidia);
    assert(classify_pci_vendor_id(0x1002U) == gpu_vendor::amd);
    assert(classify_pci_vendor_id(0x1022U) == gpu_vendor::amd);
    assert(classify_pci_vendor_id(0x8086U) == gpu_vendor::intel);
    assert(classify_pci_vendor_id(0x8087U) == gpu_vendor::intel);
    assert(classify_pci_vendor_id(0x106bU) == gpu_vendor::apple);
    assert(classify_pci_vendor_id(0x13b5U) == gpu_vendor::arm_mali);
    assert(classify_pci_vendor_id(0x5143U) == gpu_vendor::qualcomm_adreno);
    assert(classify_pci_vendor_id(0x14e4U) == gpu_vendor::broadcom_videocore);
    assert(classify_pci_vendor_id(0x1010U) == gpu_vendor::imagination_powervr);
    assert(classify_pci_vendor_id(0x1414U) == gpu_vendor::microsoft);
    assert(classify_pci_vendor_id(0x15adU) == gpu_vendor::vmware);
    assert(classify_pci_vendor_id(0x1af4U) == gpu_vendor::virtio);
    assert(classify_pci_vendor_id(0x9999U) == gpu_vendor::other);
    assert(classify_pci_vendor_id(0x0U) == gpu_vendor::unknown);

    assert(std::string_view(vendor_to_string(gpu_vendor::nvidia)) == "NVIDIA");
    assert(std::string_view(vendor_to_string(gpu_vendor::amd)) == "AMD");
    assert(std::string_view(vendor_to_string(gpu_vendor::intel)) == "Intel");
    assert(std::string_view(vendor_to_string(gpu_vendor::apple)) == "Apple");

    assert(classify_vendor_name("NVIDIA GeForce RTX 4070") == gpu_vendor::nvidia);
    assert(classify_vendor_name("AMD Radeon Graphics") == gpu_vendor::amd);
    assert(classify_vendor_name("Intel(R) Iris(R) Xe Graphics") == gpu_vendor::intel);
    assert(classify_vendor_name("Apple M2 Max GPU") == gpu_vendor::apple);
    assert(classify_vendor_name("ARM Immortalis-G715") == gpu_vendor::arm_mali);
    assert(classify_vendor_name("Qualcomm Adreno 740") == gpu_vendor::qualcomm_adreno);
    assert(classify_vendor_name("VMware SVGA 3D") == gpu_vendor::vmware);
    assert(classify_vendor_name("Red Hat VirtIO GPU") == gpu_vendor::virtio);
    assert(classify_vendor_name("Custom 3D Chip") == gpu_vendor::other);
    assert(classify_vendor_name("Creative Graphics") == gpu_vendor::other);
    assert(classify_vendor_name("Harman GPU") == gpu_vendor::other);
    assert(classify_vendor_name("") == gpu_vendor::unknown);

    // Exact driver matching tests
    assert(classify_driver_or_node("xe") == gpu_vendor::intel);
    assert(classify_driver_or_node("i915") == gpu_vendor::intel);
    assert(classify_driver_or_node("xen_drm_front") == gpu_vendor::unknown);
    assert(classify_driver_or_node("panfrost") == gpu_vendor::arm_mali);
    assert(classify_driver_or_node("vc4") == gpu_vendor::broadcom_videocore);
    assert(classify_driver_or_node("v3d") == gpu_vendor::broadcom_videocore);
    assert(classify_driver_or_node("msm") == gpu_vendor::qualcomm_adreno);
    assert(classify_driver_or_node("amdgpu") == gpu_vendor::amd);
    assert(classify_driver_or_node("nvidia") == gpu_vendor::nvidia);
}

void test_live_linux_queries() {
    const auto devs = syscape::gpu::devices();
    assert(devs);

    const auto count = syscape::gpu::device_count();
    assert(count);
    assert(devs->size() == *count);

    bool found_primary = false;
    for (const auto& dev : *devs) {
        assert(!dev.id.empty());
        assert(syscape::detail::is_valid_utf8(dev.id));
        assert(syscape::detail::is_valid_utf8(dev.vendor_name));

        if (dev.name.has_value()) {
            assert(!dev.name->empty());
            assert(syscape::detail::is_valid_utf8(*dev.name));
        }

        if (dev.driver.has_value()) {
            assert(!dev.driver->empty());
            assert(syscape::detail::is_valid_utf8(*dev.driver));
        }

        if (dev.vendor_id.has_value()) {
            const auto expected_vendor =
                syscape::detail::gpu_common::classify_pci_vendor_id(*dev.vendor_id);
            assert(dev.vendor == expected_vendor);
        }

        // VRAM may be 0 for unified memory or > 0 for dedicated VRAM
        if (dev.vram_bytes.has_value()) {
            (void)*dev.vram_bytes;
        }

        if (dev.is_primary.has_value() && *dev.is_primary) {
            found_primary = true;
        }
    }

    const auto primary = syscape::gpu::primary_device();
    if (found_primary) {
        assert(primary);
        assert(primary->is_primary.has_value() && *primary->is_primary);
        assert(syscape::detail::is_valid_utf8(primary->id));
    } else {
        assert(!primary && primary.error() == syscape::errc::not_found);
    }
}

} // namespace

int main() {
    test_synthetic_hex_parsing();
    test_synthetic_decimal_u64_parsing();
    test_synthetic_vendor_classification();
    test_live_linux_queries();
    return 0;
}
