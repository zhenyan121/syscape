#ifndef SYSCAPE_WIFI_HPP
#define SYSCAPE_WIFI_HPP

/// @file
/// @brief Hosted Wi-Fi adapters, radio state, connection, and network profile
/// queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note This module exposes:
/// - Enumeration of local Wi-Fi host adapters (adapters()).
/// - Total Wi-Fi adapter count (adapter_count()).
/// - Platform default / primary Wi-Fi adapter lookup (default_adapter()).
/// - Current active wireless connection query (current_connection()).
/// - Enumeration of saved / configured wireless network profiles
/// (configured_networks()).
/// - Adapter radio power and block states (on, off, blocked).
/// - Wi-Fi generations / standards (Legacy 802.11, Wi-Fi 4/5/6/6E/7).
/// - Frequency bands (2.4 GHz, 5 GHz, 6 GHz, 60 GHz) and channel decoding.
/// - Security / authentication protocols (Open, WEP, WPA/WPA2/WPA3 Personal and
/// Enterprise, OWE).
/// - Link rates in Mbps, signal RSSI in dBm, and signal quality percentage.
/// @note Linux queries sysfs (/sys/class/net, /sys/class/rfkill),
/// /proc/net/wireless, and Wireless Extensions (WEXT) ioctls.
/// @note Windows queries official Native Wifi APIs (wlanapi.h). WPA3 and
/// 802.11ax/802.11be decoding uses the documented numeric enum values so the
/// header does not require newer SDK spellings for those constants. Windows
/// link-rate fields remain absent because the current API documentation does
/// not specify the units of the returned rate members.
/// @note macOS currently uses the portable unsupported-capability fallback;
/// no public C++17-only system interface has been implemented and verified.
/// @note Wireless connection states can change at any time. Queries do not
/// cache results.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/wifi.hpp requires C++17 or later"
#endif

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace syscape {
namespace wifi {

/// Radio power and operational block status of a Wi-Fi adapter.
enum class adapter_power_state : std::uint8_t {
    /// Power state is unknown or could not be determined.
    unknown,
    /// Adapter is powered on and radio is active.
    on,
    /// Adapter is powered off or disabled in software.
    off,
    /// Adapter is blocked by a software or hardware rfkill switch.
    blocked
};

/// Wi-Fi generation or IEEE 802.11 physical-layer standard.
enum class wifi_standard : std::uint8_t {
    /// Standard is unknown or unspecified.
    unknown,
    /// Legacy IEEE 802.11a, 802.11b, or 802.11g.
    legacy_802_11,
    /// Wi-Fi 4 (IEEE 802.11n, High Throughput / HT).
    wifi_4,
    /// Wi-Fi 5 (IEEE 802.11ac, Very High Throughput / VHT).
    wifi_5,
    /// Wi-Fi 6 (IEEE 802.11ax, High Efficiency / HE in 2.4/5 GHz).
    wifi_6,
    /// Wi-Fi 6E (IEEE 802.11ax extended to 6 GHz band).
    wifi_6e,
    /// Wi-Fi 7 (IEEE 802.11be, Extremely High Throughput / EHT).
    wifi_7
};

/// Radio frequency band classification.
enum class frequency_band : std::uint8_t {
    /// Frequency band is unknown or outside standard bands.
    unknown,
    /// 2.4 GHz industrial, scientific, and medical (ISM) band (2400-2500 MHz).
    band_2_4_ghz,
    /// 5 GHz band (4900-5895 MHz).
    band_5_ghz,
    /// 6 GHz band (5925-7125 MHz).
    band_6_ghz,
    /// 60 GHz millimeter-wave band (IEEE 802.11ad/ay).
    band_60_ghz
};

/// Wireless security and authentication protocol.
enum class security_type : std::uint8_t {
    /// Security protocol is unknown or could not be determined.
    unknown,
    /// Open / Unsecured network (no authentication or encryption).
    open,
    /// Wired Equivalent Privacy (legacy WEP).
    wep,
    /// WPA Personal (Pre-Shared Key / TKIP/CCMP).
    wpa_personal,
    /// WPA Enterprise (802.1X EAP authentication).
    wpa_enterprise,
    /// WPA2 Personal (Pre-Shared Key / CCMP AES).
    wpa2_personal,
    /// WPA2 Enterprise (802.1X EAP authentication).
    wpa2_enterprise,
    /// WPA3 Personal (Simultaneous Authentication of Equals / SAE).
    wpa3_personal,
    /// WPA3 Enterprise (192-bit cryptographic suite / EAP).
    wpa3_enterprise,
    /// Opportunistic Wireless Encryption (OWE / Enhanced Open).
    wpa3_owe
};

/// Operational mode of the wireless interface.
enum class operation_mode : std::uint8_t {
    /// Operational mode is unknown or unspecified.
    unknown,
    /// Station / Client mode connected to an Access Point (Infrastructure).
    station,
    /// Access Point / Master / Hotspot mode.
    access_point,
    /// Independent Basic Service Set (IBSS / Ad-Hoc peer-to-peer).
    ad_hoc,
    /// Mesh Point network node (802.11s).
    mesh
};

/// Link connection state of the wireless interface.
enum class connection_state : std::uint8_t {
    /// Connection state is unknown.
    unknown,
    /// Disconnected from any network.
    disconnected,
    /// Connecting / Associating with an Access Point.
    connecting,
    /// Authenticating credentials / 802.1X handshake.
    authenticating,
    /// Connected and associated with an Access Point.
    connected
};

/// Information describing an active wireless network connection.
struct network_connection {
    /// Service Set Identifier (SSID) / network name in UTF-8.
    std::string ssid;

    /// Basic Service Set Identifier (BSSID) / MAC address of the Access Point
    /// formatted in uppercase colon-separated hex (e.g. "00:1A:2B:3C:4D:5E").
    std::string bssid;

    /// Received Signal Strength Indicator in dBm (e.g. -55 dBm), if available.
    std::optional<std::int16_t> signal_dbm;

    /// Signal link quality normalized to a percentage (0 - 100), if available.
    std::optional<std::uint8_t> signal_quality_percent;

    /// Radio center frequency in megahertz (e.g. 2412, 5180, 5955), if available.
    std::optional<std::uint32_t> frequency_mhz;

    /// Primary radio channel number (e.g. 1, 6, 11, 36, 149), if available.
    std::optional<std::uint16_t> channel;

    /// Radio frequency band classification.
    frequency_band band = frequency_band::unknown;

    /// Wi-Fi generation or physical standard in use for the connection.
    wifi_standard standard = wifi_standard::unknown;

    /// Security protocol and authentication suite in use.
    security_type security = security_type::unknown;

    /// Current physical transmit link speed in megabits per second (Mbps).
    std::optional<std::uint32_t> transmit_rate_mbps;

    /// Current physical receive link speed in megabits per second (Mbps).
    std::optional<std::uint32_t> receive_rate_mbps;
};

/// Information describing a local Wi-Fi host adapter / interface.
struct adapter_info {
    /// Unique platform identifier or interface name (e.g. "wlan0" or interface
    /// GUID).
    std::string id;

    /// Human-readable adapter name or driver description (e.g. "Intel Wi-Fi 6E
    /// AX210").
    std::string name;

    /// Canonical MAC address of the adapter in uppercase colon-separated
    /// format (e.g. "00:11:22:33:44:55"), if available.
    std::optional<std::string> mac_address;

    /// Current radio power and block status.
    adapter_power_state power_state = adapter_power_state::unknown;

    /// Operational mode of the adapter.
    operation_mode mode = operation_mode::unknown;

    /// Link connection state of the adapter.
    connection_state state = connection_state::unknown;

    /// Active network connection details if currently associated.
    std::optional<network_connection> connection;

    /// Supported Wi-Fi standards / generations, if exposed by the driver.
    std::vector<wifi_standard> supported_standards;

    /// Supported frequency bands, if exposed by the driver.
    std::vector<frequency_band> supported_bands;
};

/// Information describing a saved / configured Wi-Fi network profile.
struct configured_network {
    /// Service Set Identifier (SSID) / network profile name in UTF-8.
    std::string ssid;

    /// Specific adapter ID to which this profile is bound, or empty if global.
    std::optional<std::string> adapter_id;

    /// Configured security / authentication type.
    security_type security = security_type::unknown;

    /// Whether the operating system automatically connects to this network.
    std::optional<bool> auto_connect;

    /// Whether this network is configured as a non-broadcasting / hidden SSID.
    std::optional<bool> is_hidden;
};

} // namespace wifi
} // namespace syscape

#include <syscape/detail/wifi/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) &&           \
    !defined(__ANDROID__)
#include <syscape/detail/wifi/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/wifi/windows.hpp>
#else
#include <syscape/detail/wifi/generic.hpp>
#endif

namespace syscape {
namespace wifi {

/// Enumerates all local Wi-Fi host adapters / network interfaces.
///
/// @note The result reflects Wi-Fi adapters visible at query time. Linux and
/// Windows have backends; other platforms return not_supported.
/// @return A vector of adapter_info entries; not_supported when Wi-Fi is
/// unavailable; permission_denied when access is denied; malformed_data for
/// invalid platform data; or a native I/O error.
inline result<std::vector<adapter_info>> adapters() {
    return detail::wifi_backend::adapters();
}

/// Returns the total count of local Wi-Fi host adapters.
///
/// @note The count is a fresh snapshot and can change immediately after return.
/// @return Wi-Fi adapter count on success; or an error code describing the
/// failure.
inline result<std::size_t> adapter_count() {
    return detail::wifi_backend::adapter_count();
}

/// Returns the primary or default local Wi-Fi host adapter.
///
/// @note A backend returns the sole adapter or an unambiguous sole connected
/// adapter. It returns not_supported when multiple candidates cannot be
/// ordered by an authoritative platform source.
/// @return The default adapter_info entry; not_found if no adapters exist;
/// not_supported if the platform does not expose a default adapter concept; or
/// an error code describing the lookup failure.
inline result<adapter_info> default_adapter() {
    return detail::wifi_backend::default_adapter();
}

/// Queries the active wireless connection details.
///
/// @param adapter_id Optional adapter identifier to query. If empty, the
/// default adapter's active connection is queried.
/// @return An optional containing network_connection if connected; empty
/// optional if disconnected; not_found if the requested adapter does not
/// exist; or an error code.
inline result<std::optional<network_connection>>
current_connection(std::string_view adapter_id = {}) {
    return detail::wifi_backend::current_connection(adapter_id);
}

/// Enumerates saved / remembered Wi-Fi network profiles known to the system.
///
/// @note Queries local system profile stores non-invasively.
/// @return A vector of configured_network entries; permission_denied if profile
/// stores cannot be read; not_supported if the platform does not expose saved
/// profiles; or an error code.
inline result<std::vector<configured_network>> configured_networks() {
    return detail::wifi_backend::configured_networks();
}

} // namespace wifi
} // namespace syscape

#endif // SYSCAPE_WIFI_HPP
