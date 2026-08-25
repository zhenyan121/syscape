#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include <syscape/wifi.hpp>
#include <syscape/detail/wifi/common.hpp>
#include <syscape/detail/wifi/linux.hpp>
#include <syscape/detail/wifi/windows_profile.hpp>
#include <syscape/detail/utf8.hpp>

namespace {

void write_file(const std::string& path, const char* data, std::size_t size) {
    const int descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    assert(descriptor >= 0);
    assert(::write(descriptor, data, size) == static_cast<ssize_t>(size));
    assert(::close(descriptor) == 0);
}

void test_common_helpers() {
    using namespace syscape::detail::wifi_common;

    // Test trim_whitespace
    assert(trim_whitespace("   wlan0   ") == "wlan0");
    assert(trim_whitespace("") == "");
    assert(trim_whitespace("   ") == "");

    // Test natural_less
    assert(natural_less("wlan0", "wlan1"));
    assert(natural_less("wlan2", "wlan10"));
    assert(!natural_less("wlan10", "wlan2"));

    // Test parse_int
    const auto u32_val = parse_int<std::uint32_t>("  2412  ");
    assert(u32_val && *u32_val == 2412U);
    assert(!parse_int<std::uint32_t>(""));
    assert(!parse_int<std::uint32_t>("abc"));

    // Test parse_signed_int
    const auto s16_val = parse_signed_int<std::int16_t>(" -65 ");
    assert(s16_val && *s16_val == -65);
    const auto s16_pos = parse_signed_int<std::int16_t>(" 70 ");
    assert(s16_pos && *s16_pos == 70);

    // Test MAC formatting & normalization
    const std::uint8_t mac_bytes[6] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    assert(format_mac_bytes(mac_bytes) == "00:1A:2B:3C:4D:5E");

    const auto norm1 = normalize_mac_address("00:1a:2b:3c:4d:5e");
    assert(norm1 && *norm1 == "00:1A:2B:3C:4D:5E");
    const auto norm2 = normalize_mac_address("00-1A-2B-3C-4D-5E");
    assert(norm2 && *norm2 == "00:1A:2B:3C:4D:5E");
    const auto norm3 = normalize_mac_address("001a2b3c4d5e");
    assert(norm3 && *norm3 == "00:1A:2B:3C:4D:5E");
    assert(!normalize_mac_address(""));
    assert(!normalize_mac_address("00:11:22"));

    // Test frequency_to_band
    assert(frequency_to_band(2412) == syscape::wifi::frequency_band::band_2_4_ghz);
    assert(frequency_to_band(2484) == syscape::wifi::frequency_band::band_2_4_ghz);
    assert(frequency_to_band(5180) == syscape::wifi::frequency_band::band_5_ghz);
    assert(frequency_to_band(5955) == syscape::wifi::frequency_band::band_6_ghz);
    assert(frequency_to_band(60000) == syscape::wifi::frequency_band::band_60_ghz);
    assert(frequency_to_band(900) == syscape::wifi::frequency_band::unknown);

    // Test frequency_to_channel
    const auto ch1 = frequency_to_channel(2412);
    assert(ch1 && *ch1 == 1U);
    const auto ch6 = frequency_to_channel(2437);
    assert(ch6 && *ch6 == 6U);
    const auto ch11 = frequency_to_channel(2462);
    assert(ch11 && *ch11 == 11U);
    const auto ch14 = frequency_to_channel(2484);
    assert(ch14 && *ch14 == 14U);
    const auto ch36 = frequency_to_channel(5180);
    assert(ch36 && *ch36 == 36U);
    const auto ch6g_1 = frequency_to_channel(5955);
    assert(ch6g_1 && *ch6g_1 == 1U);

    // Test rssi_to_quality_percent
    assert(rssi_to_quality_percent(-110) == 0U);
    assert(rssi_to_quality_percent(-100) == 0U);
    assert(rssi_to_quality_percent(-75) == 50U);
    assert(rssi_to_quality_percent(-50) == 100U);
    assert(rssi_to_quality_percent(-40) == 100U);

    std::vector<syscape::wifi::adapter_info> candidates(2U);
    candidates[0].id = "wlan0";
    candidates[1].id = "wlan1";
    const auto ambiguous = select_default_adapter(candidates);
    assert(!ambiguous && ambiguous.error() == syscape::errc::not_supported);
    candidates[1].state = syscape::wifi::connection_state::connected;
    const auto selected = select_default_adapter(candidates);
    assert(selected && selected->id == "wlan1");
    candidates[0].state = syscape::wifi::connection_state::connected;
    const auto two_connected = select_default_adapter(candidates);
    assert(!two_connected &&
           two_connected.error() == syscape::errc::not_supported);
}

void test_wext_connection_helpers() {
    using namespace syscape::detail::wifi_backend;

    const std::uint8_t zero_bssid[6] = {0, 0, 0, 0, 0, 0};
    const std::uint8_t broadcast_bssid[6] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    const std::uint8_t associated_bssid[6] = {0, 1, 2, 3, 4, 5};
    assert(!is_associated_bssid(zero_bssid));
    assert(!is_associated_bssid(broadcast_bssid));
    assert(is_associated_bssid(associated_bssid));

    syscape::wifi::network_connection channel_only;
    apply_wext_frequency(wext_freq{6, 0, 0, 0}, channel_only);
    assert(channel_only.channel && *channel_only.channel == 6U);
    assert(!channel_only.frequency_mhz);
    assert(channel_only.band == syscape::wifi::frequency_band::unknown);

    syscape::wifi::network_connection frequency;
    apply_wext_frequency(wext_freq{2412, 6, 0, 0}, frequency);
    assert(frequency.frequency_mhz && *frequency.frequency_mhz == 2412U);
    assert(frequency.channel && *frequency.channel == 1U);
    assert(frequency.band == syscape::wifi::frequency_band::band_2_4_ghz);
}

void test_proc_wireless_parser() {
    using namespace syscape::detail::wifi_backend;

    const std::string_view sample_data =
        "Inter-| sta-|   Quality        |   Discarded packets               | Missed | WE\n"
        " face | tus | link level noise |  nwid  crypt   frag  retry   misc | beacon | 22\n"
        " wlan0: 0000   58.  -52.  -256        0      0      0      0      0        0\n"
        " wlan1: 0000   30.  -80.  -90        0      0      0      0      0        0\n";

    const auto entries = parse_proc_net_wireless_text(sample_data);
    assert(entries.size() == 2U);

    assert(entries[0].interface_name == "wlan0");
    assert(entries[0].status == 0U);
    assert(entries[0].link_quality && *entries[0].link_quality == 58U);
    assert(entries[0].signal_dbm && *entries[0].signal_dbm == -52);
    assert(entries[0].noise_dbm && *entries[0].noise_dbm == -256);

    assert(entries[1].interface_name == "wlan1");
    assert(entries[1].link_quality && *entries[1].link_quality == 30U);
    assert(entries[1].signal_dbm && *entries[1].signal_dbm == -80);
    assert(entries[1].noise_dbm && *entries[1].noise_dbm == -90);
}

void test_ini_parser() {
    using namespace syscape::detail::wifi_backend;

    const std::string_view nm_profile =
        "[connection]\n"
        "id=MyHomeWiFi\n"
        "type=wifi\n"
        "autoconnect=true\n"
        "\n"
        "[wifi]\n"
        "ssid=MyHomeWiFi\n"
        "mode=infrastructure\n"
        "hidden=false\n"
        "\n"
        "[wifi-security]\n"
        "key-mgmt=sae\n";

    const auto profile = parse_network_manager_profile(nm_profile);
    assert(profile && profile->has_value());
    assert((**profile).ssid == "MyHomeWiFi");
    assert((**profile).security == syscape::wifi::security_type::wpa3_personal);
    assert((**profile).auto_connect && *(**profile).auto_connect == true);
    assert((**profile).is_hidden && *(**profile).is_hidden == false);

    const auto wep = parse_network_manager_profile(
        "[wifi]\nssid=Legacy\n[wifi-security]\nkey-mgmt=none\n"
        "wep-key0=abcde\n");
    assert(wep && wep->has_value());
    assert((**wep).security == syscape::wifi::security_type::wep);

    const auto iwd = decode_iwd_ssid("=436166c3a9");
    assert(iwd && *iwd == "Caf\xC3\xA9");
    assert(!decode_iwd_ssid("=0"));
    const auto invalid_iwd = decode_iwd_ssid("=c328");
    assert(!invalid_iwd &&
           invalid_iwd.error() == syscape::errc::invalid_encoding);

    const auto escaped = decode_network_manager_string("Cafe\\sWiFi");
    assert(escaped && *escaped == "Cafe WiFi");
    const auto decimal = decode_network_manager_string("67;97;102;195;169;");
    assert(decimal && *decimal == "Caf\xC3\xA9");
    const auto invalid_bool = parse_network_manager_bool("sometimes");
    assert(!invalid_bool && invalid_bool.error() == syscape::errc::malformed_data);
}

void test_windows_profile_parser() {
    using syscape::detail::wifi_backend::parse_profile_xml;
    using syscape::wifi::security_type;

    syscape::wifi::configured_network profile;
    const auto parsed = parse_profile_xml(
        "<WLANProfile><name>Profile name</name><SSIDConfig><SSID>"
        "<name>Office&amp;WiFi</name></SSID></SSIDConfig><MSM><security>"
        "<authEncryption><authentication>WPA2</authentication>"
        "</authEncryption></security></MSM></WLANProfile>",
        profile);
    assert(parsed);
    assert(profile.ssid == "Office&WiFi");
    assert(profile.security == security_type::wpa2_enterprise);
    assert(profile.auto_connect && *profile.auto_connect);
    assert(profile.is_hidden && !*profile.is_hidden);

    syscape::wifi::configured_network hex_profile;
    const auto hex_parsed = parse_profile_xml(
        "<WLANProfile><SSIDConfig><SSID><hex>436166C3A9</hex>"
        "<name>Ignored</name></SSID><nonBroadcast>true</nonBroadcast>"
        "</SSIDConfig><connectionMode>manual</connectionMode><MSM><security>"
        "<authEncryption><authentication>WPA3SAE</authentication>"
        "</authEncryption></security></MSM></WLANProfile>",
        hex_profile);
    assert(hex_parsed);
    assert(hex_profile.ssid == "Caf\xC3\xA9");
    assert(hex_profile.security == security_type::wpa3_personal);
    assert(hex_profile.auto_connect && !*hex_profile.auto_connect);
    assert(hex_profile.is_hidden && *hex_profile.is_hidden);

    syscape::wifi::configured_network malformed;
    const auto invalid = parse_profile_xml(
        "<WLANProfile><SSIDConfig><SSID><name>Bad&bogus;</name></SSID>"
        "</SSIDConfig></WLANProfile>",
        malformed);
    assert(!invalid);
    assert(invalid.error() == syscape::errc::malformed_data);
}

void test_rfkill_adapter_correlation() {
    using syscape::detail::wifi_backend::query_rfkill_state;
    using syscape::wifi::adapter_power_state;

    char root_template[] = "/tmp/syscape-wifi-rfkill-XXXXXX";
    const char* root_value = ::mkdtemp(root_template);
    assert(root_value != nullptr);
    const std::string root(root_value);
    const std::string wlan0 = root + "/wlan0";
    const std::string wlan1 = root + "/wlan1";
    const std::string phy0 = wlan0 + "/phy80211";
    const std::string phy1 = wlan1 + "/phy80211";
    const std::string rfkill0 = phy0 + "/rfkill0";
    const std::string rfkill1 = phy1 + "/rfkill1";
    assert(::mkdir(wlan0.c_str(), 0700) == 0);
    assert(::mkdir(wlan1.c_str(), 0700) == 0);
    assert(::mkdir(phy0.c_str(), 0700) == 0);
    assert(::mkdir(phy1.c_str(), 0700) == 0);
    assert(::mkdir(rfkill0.c_str(), 0700) == 0);
    assert(::mkdir(rfkill1.c_str(), 0700) == 0);
    write_file(rfkill0 + "/hard", "0\n", 2U);
    write_file(rfkill0 + "/soft", "0\n", 2U);
    write_file(rfkill1 + "/hard", "0\n", 2U);
    write_file(rfkill1 + "/soft", "1\n", 2U);

    const auto first = query_rfkill_state("wlan0", root);
    const auto second = query_rfkill_state("wlan1", root);
    assert(first && *first == adapter_power_state::unknown);
    assert(second && *second == adapter_power_state::blocked);

    assert(::unlink((rfkill0 + "/hard").c_str()) == 0);
    assert(::unlink((rfkill0 + "/soft").c_str()) == 0);
    assert(::unlink((rfkill1 + "/hard").c_str()) == 0);
    assert(::unlink((rfkill1 + "/soft").c_str()) == 0);
    assert(::rmdir(rfkill0.c_str()) == 0);
    assert(::rmdir(rfkill1.c_str()) == 0);
    assert(::rmdir(phy0.c_str()) == 0);
    assert(::rmdir(phy1.c_str()) == 0);
    assert(::rmdir(wlan0.c_str()) == 0);
    assert(::rmdir(wlan1.c_str()) == 0);
    assert(::rmdir(root.c_str()) == 0);
}

void test_live_queries() {
    const auto adapters = syscape::wifi::adapters();
    if (!adapters) {
        // Live host state, permissions, and SSID encoding are environmental;
        // deterministic parser and failure-path behavior is tested above.
        return;
    }

    const auto count = syscape::wifi::adapter_count();
    (void)count;

    const auto def = syscape::wifi::default_adapter();
    if (def) {
        assert(!def->id.empty());
    }

    const auto conn = syscape::wifi::current_connection();
    if (conn && conn->has_value()) {
        assert(!(**conn).bssid.empty());
    }

    (void)syscape::wifi::configured_networks();
}

} // namespace

int main() {
    test_common_helpers();
    test_wext_connection_helpers();
    test_proc_wireless_parser();
    test_ini_parser();
    test_windows_profile_parser();
    test_rfkill_adapter_correlation();
    test_live_queries();
    return 0;
}
