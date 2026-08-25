#include <iostream>
#include <syscape/wifi.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_windows_wifi_backend() {
    const wchar_t unterminated[] = {L'w', L'l', L'a', L'n'};
    expect(syscape::detail::wifi_backend::bounded_wide_length(
               unterminated, 4U) == 4U,
           "Bounded wide length must not read beyond the array");
    expect(syscape::detail::wifi_backend::bounded_wide_length(
               L"wlan0", 10U) == 5U,
           "Bounded wide length must stop at the terminator");

    syscape::wifi::configured_network profile;
    const std::string_view xml_sample =
        "<?xml version=\"1.0\"?>\n"
        "<WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\">\n"
        "    <name>Office profile</name>\n"
        "    <SSIDConfig>\n"
        "        <SSID><name>Office&amp;Wifi</name></SSID>\n"
        "        <nonBroadcast>false</nonBroadcast>\n"
        "    </SSIDConfig>\n"
        "    <connectionType>ESS</connectionType>\n"
        "    <connectionMode>auto</connectionMode>\n"
        "    <MSM>\n"
        "        <security>\n"
        "            <authEncryption>\n"
        "                <authentication>WPA2PSK</authentication>\n"
        "                <encryption>AES</encryption>\n"
        "            </authEncryption>\n"
        "        </security>\n"
        "    </MSM>\n"
        "</WLANProfile>\n";

    const auto parsed =
        syscape::detail::wifi_backend::parse_profile_xml(xml_sample, profile);
    expect(parsed.has_value(), "Valid XML profile must be parsed");
    expect(profile.ssid == "Office&Wifi", "SSID name and XML entities must be parsed");
    expect(profile.security == syscape::wifi::security_type::wpa2_personal,
           "XML security must be WPA2-Personal");
    expect(profile.auto_connect.has_value() && *profile.auto_connect == true,
           "XML auto-connect must be true");
    expect(profile.is_hidden.has_value() && *profile.is_hidden == false,
           "XML is_hidden must be false");

    syscape::wifi::configured_network enterprise;
    const auto enterprise_result =
        syscape::detail::wifi_backend::parse_profile_xml(
            "<WLANProfile><name>Profile</name><SSIDConfig><SSID>"
            "<hex>436F7270</hex><name>Ignored</name></SSID></SSIDConfig>"
            "<MSM><security><authEncryption><authentication>WPA2"
            "</authentication></authEncryption></security></MSM></WLANProfile>",
            enterprise);
    expect(enterprise_result.has_value(), "Hex SSID profile must parse");
    expect(enterprise.ssid == "Corp", "Hex SSID must take precedence");
    expect(enterprise.security == syscape::wifi::security_type::wpa2_enterprise,
           "WPA2 must decode to WPA2-Enterprise");
    expect(enterprise.auto_connect && *enterprise.auto_connect,
           "Missing connectionMode must default to auto");

    using syscape::wifi::wifi_standard;
    using syscape::wifi::frequency_band;
    using syscape::wifi::security_type;
    using syscape::detail::wifi_backend::decode_phy_type;
    using syscape::detail::wifi_backend::decode_auth_algo;

    expect(decode_phy_type(static_cast<DOT11_PHY_TYPE>(10),
                           frequency_band::band_5_ghz) ==
               wifi_standard::wifi_6,
           "DOT11 HE on 5GHz must decode to Wi-Fi 6");
    expect(decode_phy_type(static_cast<DOT11_PHY_TYPE>(10),
                           frequency_band::band_6_ghz) ==
               wifi_standard::wifi_6e,
           "DOT11 HE on 6GHz must decode to Wi-Fi 6E");
    expect(decode_phy_type(dot11_phy_type_vht, frequency_band::band_5_ghz) ==
               wifi_standard::wifi_5,
           "DOT11 VHT must decode to Wi-Fi 5");
    expect(decode_phy_type(dot11_phy_type_ht, frequency_band::band_2_4_ghz) ==
               wifi_standard::wifi_4,
           "DOT11 HT must decode to Wi-Fi 4");

    expect(decode_auth_algo(DOT11_AUTH_ALGO_80211_OPEN) == security_type::open,
           "Open auth must decode to Open");
    expect(decode_auth_algo(DOT11_AUTH_ALGO_RSNA_PSK) == security_type::wpa2_personal,
           "RSNA PSK must decode to WPA2-Personal");
    expect(decode_auth_algo(static_cast<DOT11_AUTH_ALGORITHM>(9)) ==
               security_type::wpa3_personal,
           "WPA3 SAE must decode to WPA3-Personal");

    const auto adapters = syscape::wifi::adapters();
    if (adapters) {
        for (const auto& ad : *adapters) {
            expect(!ad.id.empty(), "Adapter id must not be empty");
            expect(!ad.name.empty(), "Adapter name must not be empty");
        }
    }

    const auto count = syscape::wifi::adapter_count();
    (void)count;

    const auto def = syscape::wifi::default_adapter();
    if (def) {
        expect(!def->id.empty(), "A selected adapter must have an id");
    }

    const auto conn = syscape::wifi::current_connection();
    if (conn && conn->has_value()) {
        expect(!(**conn).bssid.empty(),
               "An active Windows connection must have a BSSID");
    }

    const auto configured = syscape::wifi::configured_networks();
    if (configured) {
        for (const auto& network : *configured) {
            expect(!network.ssid.empty(),
                   "Configured Windows profiles must have an SSID");
        }
    }
}

} // namespace

int main() {
    test_windows_wifi_backend();
    return failures == 0 ? 0 : 1;
}
