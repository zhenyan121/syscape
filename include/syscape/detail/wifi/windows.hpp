#ifndef SYSCAPE_DETAIL_WIFI_WINDOWS_HPP
#define SYSCAPE_DETAIL_WIFI_WINDOWS_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <wlanapi.h>
#include <windot11.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <syscape/wifi.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/detail/wifi/common.hpp>
#include <syscape/detail/wifi/windows_profile.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace wifi_backend {

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

inline std::string guid_to_string(const GUID& guid) {
    char buffer[64] = {0};
    std::snprintf(buffer, sizeof(buffer),
                  "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                  static_cast<unsigned long>(guid.Data1),
                  static_cast<unsigned int>(guid.Data2),
                  static_cast<unsigned int>(guid.Data3),
                  static_cast<unsigned int>(guid.Data4[0]),
                  static_cast<unsigned int>(guid.Data4[1]),
                  static_cast<unsigned int>(guid.Data4[2]),
                  static_cast<unsigned int>(guid.Data4[3]),
                  static_cast<unsigned int>(guid.Data4[4]),
                  static_cast<unsigned int>(guid.Data4[5]),
                  static_cast<unsigned int>(guid.Data4[6]),
                  static_cast<unsigned int>(guid.Data4[7]));
    return std::string(buffer);
}

inline wifi::wifi_standard decode_phy_type(DOT11_PHY_TYPE phy_type,
                                           wifi::frequency_band band) noexcept {
    switch (phy_type) {
    // Values 10 and 11 are the documented HE and EHT enum values. Using the
    // values keeps this header source-compatible with older SDK headers that
    // predate the corresponding enumerator names.
    case static_cast<DOT11_PHY_TYPE>(10): // 802.11ax HE
        if (band == wifi::frequency_band::band_6_ghz) {
            return wifi::wifi_standard::wifi_6e;
        }
        if (band == wifi::frequency_band::band_2_4_ghz ||
            band == wifi::frequency_band::band_5_ghz) {
            return wifi::wifi_standard::wifi_6;
        }
        return wifi::wifi_standard::unknown;
    case static_cast<DOT11_PHY_TYPE>(11): // 802.11be EHT
        return wifi::wifi_standard::wifi_7;
    case dot11_phy_type_vht: // 802.11ac
        return wifi::wifi_standard::wifi_5;
    case dot11_phy_type_ht: // 802.11n
        return wifi::wifi_standard::wifi_4;
    case dot11_phy_type_erp:
    case dot11_phy_type_ofdm:
    case dot11_phy_type_hrdsss:
    case dot11_phy_type_dsss:
    case dot11_phy_type_fhss:
        return wifi::wifi_standard::legacy_802_11;
    default:
        return wifi::wifi_standard::unknown;
    }
}

inline wifi::security_type
decode_auth_algo(DOT11_AUTH_ALGORITHM auth_algo) noexcept {
    switch (auth_algo) {
    case DOT11_AUTH_ALGO_80211_OPEN:
        return wifi::security_type::open;
    case DOT11_AUTH_ALGO_80211_SHARED_KEY:
        return wifi::security_type::wep;
    case DOT11_AUTH_ALGO_WPA:
        return wifi::security_type::wpa_enterprise;
    case DOT11_AUTH_ALGO_WPA_PSK:
        return wifi::security_type::wpa_personal;
    case DOT11_AUTH_ALGO_RSNA:
        return wifi::security_type::wpa2_enterprise;
    case DOT11_AUTH_ALGO_RSNA_PSK:
        return wifi::security_type::wpa2_personal;
    // Values 8 through 11 are documented by DOT11_AUTH_ALGORITHM but their
    // names are absent from older Windows SDK and MinGW header sets.
    case static_cast<DOT11_AUTH_ALGORITHM>(9):
        return wifi::security_type::wpa3_personal;
    case static_cast<DOT11_AUTH_ALGORITHM>(10):
        return wifi::security_type::wpa3_owe;
    case static_cast<DOT11_AUTH_ALGORITHM>(8):
    case static_cast<DOT11_AUTH_ALGORITHM>(11):
        return wifi::security_type::wpa3_enterprise;
    default:
        return wifi::security_type::unknown;
    }
}

inline wifi::connection_state
decode_interface_state(WLAN_INTERFACE_STATE state) noexcept {
    switch (state) {
    case wlan_interface_state_connected:
        return wifi::connection_state::connected;
    case wlan_interface_state_disconnected:
        return wifi::connection_state::disconnected;
    case wlan_interface_state_associating:
        return wifi::connection_state::connecting;
    case wlan_interface_state_authenticating:
        return wifi::connection_state::authenticating;
    default:
        return wifi::connection_state::unknown;
    }
}

class wlan_client_guard {
public:
    wlan_client_guard() noexcept : handle_(nullptr), error_(ERROR_SUCCESS) {
        DWORD negotiated_version = 0;
        const DWORD res = ::WlanOpenHandle(2, nullptr, &negotiated_version, &handle_);
        if (res != ERROR_SUCCESS) {
            handle_ = nullptr;
            error_ = res;
        }
    }

    ~wlan_client_guard() noexcept {
        if (handle_ != nullptr) {
            ::WlanCloseHandle(handle_, nullptr);
        }
    }

    wlan_client_guard(const wlan_client_guard&) = delete;
    wlan_client_guard& operator=(const wlan_client_guard&) = delete;

    HANDLE get() const noexcept { return handle_; }
    bool valid() const noexcept { return handle_ != nullptr; }
    DWORD error() const noexcept { return error_; }

private:
    HANDLE handle_;
    DWORD error_;
};

class wlan_memory_guard {
public:
    explicit wlan_memory_guard(void* value) noexcept : value_(value) {}
    ~wlan_memory_guard() noexcept {
        if (value_ != nullptr) {
            ::WlanFreeMemory(value_);
        }
    }
    wlan_memory_guard(const wlan_memory_guard&) = delete;
    wlan_memory_guard& operator=(const wlan_memory_guard&) = delete;

private:
    void* value_;
};

inline void query_connection_radio_details(
    HANDLE client, const GUID& interface_guid,
    const DOT11_MAC_ADDRESS& connected_bssid,
    wifi::network_connection& connection) noexcept {
    DWORD data_size = 0U;
    PULONG channel_number = nullptr;
    WLAN_OPCODE_VALUE_TYPE value_type;
    const DWORD channel_result = ::WlanQueryInterface(
        client, &interface_guid, wlan_intf_opcode_channel_number, nullptr,
        &data_size, reinterpret_cast<PVOID*>(&channel_number), &value_type);
    const wlan_memory_guard channel_guard(channel_number);
    if (channel_result == ERROR_SUCCESS && channel_number != nullptr &&
        data_size >= sizeof(ULONG) &&
        *channel_number <=
            static_cast<ULONG>((std::numeric_limits<std::uint16_t>::max)())) {
        connection.channel = static_cast<std::uint16_t>(*channel_number);
    }

    PWLAN_BSS_LIST bss_list = nullptr;
    const DWORD bss_result = ::WlanGetNetworkBssList(
        client, &interface_guid, nullptr, dot11_BSS_type_any, FALSE, nullptr,
        &bss_list);
    const wlan_memory_guard bss_guard(bss_list);
    if (bss_result != ERROR_SUCCESS || bss_list == nullptr ||
        bss_list->dwTotalSize < offsetof(WLAN_BSS_LIST, wlanBssEntries)) {
        return;
    }
    const std::size_t available_entries =
        (bss_list->dwTotalSize - offsetof(WLAN_BSS_LIST, wlanBssEntries)) /
        sizeof(WLAN_BSS_ENTRY);
    const std::size_t entry_count = (std::min)(
        static_cast<std::size_t>(bss_list->dwNumberOfItems),
        available_entries);
    for (std::size_t index = 0U; index < entry_count; ++index) {
        const auto& entry = bss_list->wlanBssEntries[index];
        if (!std::equal(std::begin(entry.dot11Bssid),
                        std::end(entry.dot11Bssid),
                        std::begin(connected_bssid))) {
            continue;
        }
        if (entry.ulChCenterFrequency == 0U ||
            entry.ulChCenterFrequency % 1000U != 0U) {
            return;
        }
        const std::uint32_t frequency_mhz =
            entry.ulChCenterFrequency / 1000U;
        const auto band = wifi_common::frequency_to_band(frequency_mhz);
        if (band == wifi::frequency_band::unknown) {
            return;
        }
        connection.frequency_mhz = frequency_mhz;
        connection.band = band;
        const auto derived_channel =
            wifi_common::frequency_to_channel(frequency_mhz);
        if (derived_channel) {
            connection.channel = derived_channel;
        }
        return;
    }
}

inline result<std::vector<wifi::adapter_info>> adapters() {
    wlan_client_guard client;
    if (!client.valid()) {
        if (client.error() == ERROR_ACCESS_DENIED) {
            return fail(errc::permission_denied);
        }
        if (client.error() == ERROR_SERVICE_NOT_ACTIVE) {
            return fail(errc::temporarily_unavailable);
        }
        if (client.error() == ERROR_NOT_SUPPORTED) {
            return fail(errc::not_supported);
        }
        return fail(windows_error(client.error()));
    }

    PWLAN_INTERFACE_INFO_LIST pIfList = nullptr;
    const DWORD enum_res = ::WlanEnumInterfaces(client.get(), nullptr, &pIfList);
    if (enum_res != ERROR_SUCCESS) {
        if (enum_res == ERROR_ACCESS_DENIED) {
            return fail(errc::permission_denied);
        }
        if (enum_res == ERROR_SERVICE_NOT_ACTIVE) {
            return fail(errc::temporarily_unavailable);
        }
        if (enum_res == ERROR_NOT_SUPPORTED) {
            return fail(errc::not_supported);
        }
        return fail(windows_error(enum_res));
    }

    std::vector<wifi::adapter_info> result_list;
    if (pIfList == nullptr) {
        return result_list;
    }
    const wlan_memory_guard interface_guard(pIfList);

    result_list.reserve(pIfList->dwNumberOfItems);

    for (DWORD i = 0; i < pIfList->dwNumberOfItems; ++i) {
        const auto& item = pIfList->InterfaceInfo[i];
        wifi::adapter_info info;
        info.id = guid_to_string(item.InterfaceGuid);

        const auto name_utf8 = wide_to_utf8(std::wstring_view(
            item.strInterfaceDescription,
            bounded_wide_length(item.strInterfaceDescription,
                                sizeof(item.strInterfaceDescription) /
                                    sizeof(item.strInterfaceDescription[0]))));
        if (name_utf8) {
            info.name = name_utf8->empty() ? info.id : *name_utf8;
        } else {
            return fail(name_utf8.error());
        }

        info.state = decode_interface_state(item.isState);

        // Query radio state
        DWORD radio_size = 0;
        PWLAN_PHY_RADIO_STATE pRadio = nullptr;
        WLAN_OPCODE_VALUE_TYPE opType;
        const DWORD radio_result = ::WlanQueryInterface(
            client.get(), &item.InterfaceGuid, wlan_intf_opcode_radio_state,
            nullptr, &radio_size, reinterpret_cast<PVOID*>(&pRadio), &opType);
        const wlan_memory_guard radio_guard(pRadio);
        if (radio_result == ERROR_SUCCESS && pRadio == nullptr) {
            return fail(errc::malformed_data);
        }
        if (radio_result == ERROR_SUCCESS) {
            if (pRadio->dwNumberOfPhys > 0) {
                bool all_off = true;
                bool all_on = true;
                for (DWORD phy_index = 0U;
                     phy_index < pRadio->dwNumberOfPhys;
                     ++phy_index) {
                    const auto& phy_radio = pRadio->PhyRadioState[phy_index];
                    const bool off =
                        phy_radio.dot11HardwareRadioState == dot11_radio_state_off ||
                        phy_radio.dot11SoftwareRadioState == dot11_radio_state_off;
                    all_off = all_off && off;
                    all_on = all_on && !off &&
                        phy_radio.dot11HardwareRadioState == dot11_radio_state_on &&
                        phy_radio.dot11SoftwareRadioState == dot11_radio_state_on;
                }
                if (all_off) info.power_state = wifi::adapter_power_state::off;
                else if (all_on) info.power_state = wifi::adapter_power_state::on;
            }
        }

        // Query current connection
        if (info.state == wifi::connection_state::connected) {
            DWORD conn_size = 0;
            PWLAN_CONNECTION_ATTRIBUTES pConn = nullptr;
            const DWORD connection_result = ::WlanQueryInterface(
                client.get(), &item.InterfaceGuid,
                wlan_intf_opcode_current_connection, nullptr, &conn_size,
                reinterpret_cast<PVOID*>(&pConn), &opType);
            const wlan_memory_guard connection_guard(pConn);
            if (connection_result != ERROR_SUCCESS) {
                // Interface state can change between enumeration and this
                // query. A per-interface failure must not discard the other
                // adapters in the snapshot.
                if (connection_result == ERROR_INVALID_STATE) {
                    info.state = wifi::connection_state::disconnected;
                } else {
                    info.state = wifi::connection_state::unknown;
                }
            } else if (pConn == nullptr) {
                return fail(errc::malformed_data);
            } else {
                wifi::network_connection conn;
                // SSID
                if (pConn->wlanAssociationAttributes.dot11Ssid.uSSIDLength > 0 &&
                    pConn->wlanAssociationAttributes.dot11Ssid.uSSIDLength <=
                        DOT11_SSID_MAX_LENGTH) {
                    const std::string_view ssid_view(
                        reinterpret_cast<const char*>(
                            pConn->wlanAssociationAttributes.dot11Ssid.ucSSID),
                        pConn->wlanAssociationAttributes.dot11Ssid.uSSIDLength);
                    if (!is_valid_utf8(ssid_view)) {
                        return fail(errc::invalid_encoding);
                    }
                    conn.ssid = std::string(ssid_view);
                }
                // BSSID
                conn.bssid = wifi_common::format_mac_bytes(
                    pConn->wlanAssociationAttributes.dot11Bssid);

                query_connection_radio_details(
                    client.get(), item.InterfaceGuid,
                    pConn->wlanAssociationAttributes.dot11Bssid, conn);

                // Signal Quality
                conn.signal_quality_percent = static_cast<std::uint8_t>(
                    (std::min)(pConn->wlanAssociationAttributes.wlanSignalQuality, 100UL));
                conn.signal_dbm = static_cast<std::int16_t>(
                    static_cast<int>(conn.signal_quality_percent.value()) / 2 - 100);

                // PHY Type & Band
                conn.standard = decode_phy_type(
                    pConn->wlanAssociationAttributes.dot11PhyType,
                    conn.band);

                // Security
                conn.security = decode_auth_algo(
                    pConn->wlanSecurityAttributes.dot11AuthAlgorithm);

                // The current Microsoft documentation does not define units
                // for ulTxRate/ulRxRate, so they remain absent until verified
                // against a supported Windows SDK and real hardware.

                info.connection = std::move(conn);
            }
        }

        result_list.push_back(std::move(info));
    }

    return result_list;
}

inline result<std::size_t> adapter_count() {
    const auto list = adapters();
    if (!list) {
        return fail(list.error());
    }
    return list->size();
}

inline result<wifi::adapter_info> default_adapter() {
    const auto list = adapters();
    if (!list) {
        return fail(list.error());
    }
    return wifi_common::select_default_adapter(*list);
}

inline result<std::optional<wifi::network_connection>>
current_connection(std::string_view adapter_id) {
    if (adapter_id.empty()) {
        const auto selected = default_adapter();
        if (!selected) {
            return fail(selected.error());
        }
        return selected->connection;
    }
    const auto list = adapters();
    if (!list) {
        return fail(list.error());
    }
    if (list->empty()) {
        return fail(errc::not_found);
    }

    for (const auto& adapter : *list) {
        if (adapter.id == adapter_id) {
            return adapter.connection;
        }
    }
    return fail(errc::not_found);
}

inline result<std::vector<wifi::configured_network>> configured_networks() {
    wlan_client_guard client;
    if (!client.valid()) {
        if (client.error() == ERROR_ACCESS_DENIED) {
            return fail(errc::permission_denied);
        }
        if (client.error() == ERROR_SERVICE_NOT_ACTIVE) {
            return fail(errc::temporarily_unavailable);
        }
        if (client.error() == ERROR_NOT_SUPPORTED) {
            return fail(errc::not_supported);
        }
        return fail(windows_error(client.error()));
    }

    PWLAN_INTERFACE_INFO_LIST pIfList = nullptr;
    const DWORD enum_result =
        ::WlanEnumInterfaces(client.get(), nullptr, &pIfList);
    if (enum_result != ERROR_SUCCESS) {
        if (enum_result == ERROR_ACCESS_DENIED) {
            return fail(errc::permission_denied);
        }
        if (enum_result == ERROR_SERVICE_NOT_ACTIVE) {
            return fail(errc::temporarily_unavailable);
        }
        if (enum_result == ERROR_NOT_SUPPORTED) {
            return fail(errc::not_supported);
        }
        return fail(windows_error(enum_result));
    }

    std::vector<wifi::configured_network> profiles;
    if (pIfList == nullptr) {
        return profiles;
    }
    const wlan_memory_guard interface_guard(pIfList);

    for (DWORD i = 0; i < pIfList->dwNumberOfItems; ++i) {
        const auto& item = pIfList->InterfaceInfo[i];
        PWLAN_PROFILE_INFO_LIST pProfileList = nullptr;
        const DWORD list_result = ::WlanGetProfileList(
            client.get(), &item.InterfaceGuid, nullptr, &pProfileList);
        if (list_result != ERROR_SUCCESS) {
            if (list_result == ERROR_ACCESS_DENIED) {
                return fail(errc::permission_denied);
            }
            if (list_result == ERROR_SERVICE_NOT_ACTIVE) {
                return fail(errc::temporarily_unavailable);
            }
            if (list_result == ERROR_NOT_SUPPORTED) {
                return fail(errc::not_supported);
            }
            return fail(windows_error(list_result));
        }
        if (pProfileList == nullptr) {
            return fail(errc::malformed_data);
        }
        const wlan_memory_guard profile_list_guard(pProfileList);
        for (DWORD j = 0; j < pProfileList->dwNumberOfItems; ++j) {
            const auto& pinfo = pProfileList->ProfileInfo[j];
            LPWSTR pXml = nullptr;
            DWORD flags = 0;
            DWORD access = 0;
            const DWORD profile_result = ::WlanGetProfile(
                client.get(), &item.InterfaceGuid, pinfo.strProfileName,
                nullptr, &pXml, &flags, &access);
            if (profile_result != ERROR_SUCCESS) {
                if (profile_result == ERROR_ACCESS_DENIED) {
                    return fail(errc::permission_denied);
                }
                if (profile_result == ERROR_SERVICE_NOT_ACTIVE) {
                    return fail(errc::temporarily_unavailable);
                }
                if (profile_result == ERROR_NOT_SUPPORTED) {
                    return fail(errc::not_supported);
                }
                return fail(windows_error(profile_result));
            }
            if (pXml == nullptr) {
                return fail(errc::malformed_data);
            }
            const wlan_memory_guard xml_guard(pXml);
            const auto xml_utf8 = wide_to_utf8(std::wstring_view(pXml));
            if (!xml_utf8) {
                return fail(xml_utf8.error());
            }
            wifi::configured_network profile;
            profile.adapter_id = guid_to_string(item.InterfaceGuid);
            const auto parsed = parse_profile_xml(*xml_utf8, profile);
            if (!parsed) {
                return fail(parsed.error());
            }
            if (profile.ssid.empty()) {
                return fail(errc::malformed_data);
            }
            profiles.push_back(std::move(profile));
        }
    }

    return profiles;
}

} // namespace wifi_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_WIFI_WINDOWS_HPP
