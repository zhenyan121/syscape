#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

#include <syscape/display.hpp>
#include <syscape/detail/display/common.hpp>
#include <syscape/detail/display/linux.hpp>
#include <syscape/detail/utf8.hpp>

namespace {

void set_edid_checksum(std::uint8_t* data) {
    std::uint8_t sum = 0U;
    for (std::size_t i = 0U; i < 127U; ++i) {
        sum = static_cast<std::uint8_t>(sum + data[i]);
    }
    data[127] = static_cast<std::uint8_t>(0U - sum);
}

std::vector<std::uint8_t> make_maximum_sized_edid() {
    using namespace syscape::detail::display_common;
    std::vector<std::uint8_t> data(maximum_edid_size, 0U);
    const std::uint8_t header[8] = {
        0x00U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x00U
    };
    std::memcpy(data.data(), header, sizeof(header));
    data[126] = 255U;
    set_edid_checksum(data.data());

    for (std::size_t block = 1U; block < 256U; ++block) {
        std::uint8_t* extension = data.data() + block * edid_block_size;
        extension[0] = 0x02U;
        extension[1] = 0x03U;
        extension[2] = 0x04U;
        set_edid_checksum(extension);
    }
    return data;
}

void test_synthetic_mode_parsing() {
    using namespace syscape::detail::display_common;

    const auto m1 = parse_mode_resolution("2560x1600");
    assert(m1 && m1->first == 2560U && m1->second == 1600U);

    const auto m2 = parse_mode_resolution("1920X1080");
    assert(m2 && m2->first == 1920U && m2->second == 1080U);

    const auto m3 = parse_mode_resolution(" 3840x2160 \n");
    assert(m3 && m3->first == 3840U && m3->second == 2160U);

    const auto m_inv1 = parse_mode_resolution("");
    assert(!m_inv1 && m_inv1.error() == syscape::errc::malformed_data);

    const auto m_inv2 = parse_mode_resolution("2560");
    assert(!m_inv2 && m_inv2.error() == syscape::errc::malformed_data);

    const auto m_inv3 = parse_mode_resolution("2560x");
    assert(!m_inv3 && m_inv3.error() == syscape::errc::malformed_data);

    const auto m_inv4 = parse_mode_resolution("x1600");
    assert(!m_inv4 && m_inv4.error() == syscape::errc::malformed_data);

    const auto m_inv5 = parse_mode_resolution("2560xabc");
    assert(!m_inv5 && m_inv5.error() == syscape::errc::malformed_data);

    const auto m_inv6 = parse_mode_resolution("0x1080");
    assert(!m_inv6 && m_inv6.error() == syscape::errc::malformed_data);

    const auto m_inv7 = parse_mode_resolution("1920x0");
    assert(!m_inv7 && m_inv7.error() == syscape::errc::malformed_data);
}

void test_synthetic_edid_parsing() {
    using namespace syscape::detail::display_common;

    // Construct a synthetic valid 128-byte EDID 1.4 block
    std::uint8_t edid_raw[128];
    std::memset(edid_raw, 0, sizeof(edid_raw));

    // Header: 00 FF FF FF FF FF FF 00
    edid_raw[0] = 0x00U;
    edid_raw[1] = 0xFFU;
    edid_raw[2] = 0xFFU;
    edid_raw[3] = 0xFFU;
    edid_raw[4] = 0xFFU;
    edid_raw[5] = 0xFFU;
    edid_raw[6] = 0xFFU;
    edid_raw[7] = 0x00U;

    // Manufacturer ID: "DEL" (D = 4, E = 5, L = 12)
    // mfg_raw = (4 << 10) | (5 << 5) | 12 = 4096 + 160 + 12 = 4268 = 0x10AC
    edid_raw[8] = 0x10U;
    edid_raw[9] = 0xACU;

    // Product Code: 0x40B2 (little-endian: 0xB2, 0x40)
    edid_raw[10] = 0xB2U;
    edid_raw[11] = 0x40U;

    // Screen size: 60 cm x 34 cm (600mm x 340mm)
    edid_raw[21] = 60U;
    edid_raw[22] = 34U;

    // Descriptor 1 (offset 54): Detailed Timing Descriptor
    // 3840x2160 @ 60Hz -> Pixel clock 587.40 MHz (58740 in 10kHz = 0xE574)
    // H total: 3840 + 560 = 4400, V total: 2160 + 65 = 2225 -> 4400 * 2225 * 60 = 587,400,000 Hz
    edid_raw[54] = 0x74U;
    edid_raw[55] = 0xE5U;
    // H active: 3840 (0xF00), blanking: 560 (0x230)
    edid_raw[56] = 0x00U; // h_active lower 8 bits
    edid_raw[57] = 0x30U; // h_blanking lower 8 bits
    edid_raw[58] = 0xF2U; // (0xF << 4) | 0x2
    // V active: 2160 (0x870), blanking: 65 (0x041)
    edid_raw[59] = 0x70U; // v_active lower 8 bits
    edid_raw[60] = 0x41U; // v_blanking lower 8 bits
    edid_raw[61] = 0x80U; // (0x8 << 4) | 0x0

    // Descriptor 2 (offset 72): Monitor Name Descriptor (Tag 0xFC)
    edid_raw[72] = 0x00U;
    edid_raw[73] = 0x00U;
    edid_raw[74] = 0x00U;
    edid_raw[75] = 0xFCU; // Tag
    edid_raw[76] = 0x00U;
    const char name_str[] = "DELL U2720Q\n";
    std::memcpy(edid_raw + 77, name_str, sizeof(name_str) - 1U);

    set_edid_checksum(edid_raw);

    const auto facts = parse_edid_block(edid_raw, sizeof(edid_raw));
    assert(facts);
    assert(facts->manufacturer.has_value() && *facts->manufacturer == "DEL");
    assert(facts->product_code.has_value() && *facts->product_code == 0x40B2U);
    assert(facts->physical_width_mm.has_value() && *facts->physical_width_mm == 600U);
    assert(facts->physical_height_mm.has_value() && *facts->physical_height_mm == 340U);
    assert(facts->monitor_name.has_value() && *facts->monitor_name == "DELL U2720Q");
    assert(facts->preferred_width.has_value() && *facts->preferred_width == 3840U);
    assert(facts->preferred_height.has_value() && *facts->preferred_height == 2160U);
    assert(facts->preferred_refresh_rate_hz.has_value());
    assert(std::abs(*facts->preferred_refresh_rate_hz - 60.0) < 1.0);

    // A block with a valid header but a bad checksum is malformed.
    edid_raw[20] ^= 0x01U;
    const auto err_checksum = parse_edid_block(edid_raw, sizeof(edid_raw));
    assert(!err_checksum && err_checksum.error() == syscape::errc::malformed_data);
    edid_raw[20] ^= 0x01U;

    // Corrupt header test
    edid_raw[0] = 0x01U;
    const auto err_hdr = parse_edid_block(edid_raw, sizeof(edid_raw));
    assert(!err_hdr && err_hdr.error() == syscape::errc::malformed_data);

    // Truncated size test
    const auto err_trunc = parse_edid_block(edid_raw, 64U);
    assert(!err_trunc && err_trunc.error() == syscape::errc::malformed_data);

    // Null pointer test
    const auto err_null = parse_edid_block(nullptr, sizeof(edid_raw));
    assert(!err_null && err_null.error() == syscape::errc::malformed_data);
}

void test_extended_edid_validation() {
    using namespace syscape::detail::display_common;
    std::vector<std::uint8_t> edid = make_maximum_sized_edid();
    const auto maximum = parse_edid_block(edid.data(), edid.size());
    assert(maximum);

    const auto truncated = parse_edid_block(edid.data(), edid.size() - edid_block_size);
    assert(!truncated && truncated.error() == syscape::errc::malformed_data);

    edid[edid_block_size + 10U] ^= 0x01U;
    const auto bad_extension = parse_edid_block(edid.data(), edid.size());
    assert(!bad_extension && bad_extension.error() == syscape::errc::malformed_data);
}

void test_connector_does_not_invent_desktop_state() {
    const std::filesystem::path fixture =
        std::filesystem::temp_directory_path() /
        ("syscape-display-" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(fixture);
    std::filesystem::create_directories(fixture);
    {
        std::ofstream status(fixture / "status");
        status << "connected\n";
        std::ofstream modes(fixture / "modes");
        modes << "1920x1080\n1280x720\n";
        const std::vector<std::uint8_t> edid_data = make_maximum_sized_edid();
        std::ofstream edid(fixture / "edid", std::ios::binary);
        edid.write(reinterpret_cast<const char*>(edid_data.data()),
                   static_cast<std::streamsize>(edid_data.size()));
    }

    const auto info = syscape::detail::display_backend::inspect_connector(
        "card0-HDMI-A-1", fixture.string());
    std::filesystem::remove_all(fixture);

    assert(info);
    assert(info->supported_modes.size() == 2U);
    assert(!info->current_width.has_value());
    assert(!info->current_height.has_value());
    assert(!info->refresh_rate_hz.has_value());
    assert(!info->bounds.has_value());
    assert(!info->orientation.has_value());
    assert(!info->is_primary);
}

void test_connector_classification() {
    using namespace syscape::detail::display_common;
    using syscape::detail::display_backend::is_connector_name;

    assert(is_connector_name("card0-DP-1"));
    assert(is_connector_name("card1-eDP-1"));
    assert(is_connector_name("card0-HDMI-A-1"));
    assert(!is_connector_name("card0"));
    assert(!is_connector_name("card1"));
    assert(!is_connector_name("renderD128"));
    assert(!is_connector_name("version"));
    assert(!is_connector_name(""));

    std::optional<std::string> type;
    bool is_int = false;

    classify_connector_name("card0-DP-1", type, is_int);
    assert(type.has_value() && *type == "DP");
    assert(!is_int);

    classify_connector_name("card1-eDP-1", type, is_int);
    assert(type.has_value() && *type == "eDP");
    assert(is_int);

    classify_connector_name("card0-HDMI-A-1", type, is_int);
    assert(type.has_value() && *type == "HDMI-A");
    assert(!is_int);

    classify_connector_name("card0-LVDS-1", type, is_int);
    assert(type.has_value() && *type == "LVDS");
    assert(is_int);

    classify_connector_name("card0-DSI-1", type, is_int);
    assert(type.has_value() && *type == "DSI");
    assert(is_int);

    classify_connector_name("card1-Writeback-1", type, is_int);
    assert(type.has_value() && *type == "Writeback");
    assert(!is_int);
}

void test_live_linux_queries() {
    const auto disps = syscape::display::displays();
    assert(disps);

    const auto count = syscape::display::display_count();
    assert(count);
    assert(disps->size() == *count);

    bool found_primary = false;
    for (const auto& d : *disps) {
        assert(!d.id.empty());
        assert(syscape::detail::is_valid_utf8(d.id));

        if (d.name.has_value()) {
            assert(!d.name->empty());
            assert(syscape::detail::is_valid_utf8(*d.name));
        }

        if (d.manufacturer.has_value()) {
            assert(d.manufacturer->size() == 3U);
            assert(syscape::detail::is_valid_utf8(*d.manufacturer));
        }

        if (d.connector_type.has_value()) {
            assert(!d.connector_type->empty());
            assert(syscape::detail::is_valid_utf8(*d.connector_type));
        }

        if (d.bounds.has_value()) {
            assert(d.bounds->width > 0U);
            assert(d.bounds->height > 0U);
        }

        if (d.current_width.has_value()) {
            assert(*d.current_width > 0U);
        }
        if (d.current_height.has_value()) {
            assert(*d.current_height > 0U);
        }

        if (d.refresh_rate_hz.has_value()) {
            assert(*d.refresh_rate_hz > 0.0);
        }

        if (d.physical_width_mm.has_value() && d.physical_height_mm.has_value()) {
            assert(*d.physical_width_mm > 0U);
            assert(*d.physical_height_mm > 0U);
        }

        for (const auto& m : d.supported_modes) {
            assert(m.width > 0U);
            assert(m.height > 0U);
            if (m.refresh_rate_hz.has_value()) {
                assert(*m.refresh_rate_hz > 0.0);
            }
        }

        if (d.is_primary) {
            found_primary = true;
        }
    }

    const auto primary = syscape::display::primary_display();
    if (found_primary) {
        assert(primary);
        assert(primary->is_primary);
        assert(syscape::detail::is_valid_utf8(primary->id));
    } else {
        assert(!primary && primary.error() == syscape::errc::not_found);
    }
}

} // namespace

int main() {
    test_synthetic_mode_parsing();
    test_synthetic_edid_parsing();
    test_extended_edid_validation();
    test_connector_classification();
    test_connector_does_not_invent_desktop_state();
    test_live_linux_queries();
    return 0;
}
