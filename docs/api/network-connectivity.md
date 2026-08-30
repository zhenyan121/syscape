[English](network-connectivity.md) | [简体中文](network-connectivity.zh-CN.md)

# Network and Connectivity API Reference

The Network and Connectivity headers provide introspection of network interfaces, IP routing, DNS resolver configuration, active TCP/UDP connections, listening socket endpoints, Wi-Fi adapters, and Bluetooth host controllers.

All headers in this group require the **Hosted Full** profile with **strict C++17**.

---

## 1. Network Interfaces, Routes, and DNS (`<syscape/network.hpp>`)

- **Profile**: Hosted Full (C++17)
- **Namespace**: `namespace syscape::network`

### Types and Enums

#### Enums
- `enum class address_family`: `ipv4`, `ipv6`.
- `enum class interface_state`: `unknown`, `up`, `down`, `dormant`, `testing`, `lower_layer_down`.

#### Structs
```cpp
struct unicast_address {
    address_family family;
    std::array<unsigned char, 16> value;
    std::uint8_t prefix_length;
    std::uint32_t scope_id;
};

struct interface_entry {
    std::string name;
    std::uint32_t index;
    interface_state state;
    bool loopback;
    std::vector<std::uint8_t> hardware_address;
    std::vector<unicast_address> addresses;
    std::uint32_t mtu_bytes;
};

struct ip_address {
    address_family family;
    std::array<unsigned char, 16> value;
    std::uint32_t scope_id;
};

struct route_entry {
    ip_address destination;
    std::uint8_t prefix_length;
    std::optional<ip_address> next_hop;
    std::uint32_t interface_index;
    std::optional<std::uint32_t> metric;
};

struct gateway_entry {
    ip_address address;
    std::uint32_t interface_index;
    std::optional<std::uint32_t> metric;
};

struct dns_server_entry {
    ip_address address;
    std::optional<std::uint32_t> interface_index;
};

struct dns_configuration {
    std::vector<dns_server_entry> servers;
    std::optional<std::vector<std::string>> search_domains;
    std::optional<std::string> domain_name;
};

struct interface_statistics {
    std::string name;
    std::uint32_t index = 0U;
    std::uint64_t rx_bytes = 0U;
    std::uint64_t tx_bytes = 0U;
    std::uint64_t rx_packets = 0U;
    std::uint64_t tx_packets = 0U;
    std::uint64_t rx_errors = 0U;
    std::uint64_t tx_errors = 0U;
    std::uint64_t rx_dropped = 0U;
    std::uint64_t tx_dropped = 0U;
    std::optional<std::uint64_t> rx_multicast;
    std::optional<std::uint64_t> collisions;
};
```

### Functions

```cpp
result<std::vector<interface_entry>> interfaces();
result<std::vector<route_entry>> routes();
result<std::vector<gateway_entry>> default_gateways();
result<dns_configuration> dns();

result<std::vector<interface_statistics>> statistics();
result<interface_statistics> statistics(std::string_view interface_name);
result<interface_statistics> statistics(std::uint32_t interface_index);
```

---

## 2. Active Sockets and Connections (`<syscape/connection.hpp>`)

- **Profile**: Hosted Full (C++17)
- **Namespace**: `namespace syscape::connection`

### Types and Enums

#### Enums
- `enum class protocol`: `tcp`, `udp`.
- `enum class tcp_state`: `unknown`, `closed`, `listen`, `syn_sent`, `syn_received`, `established`, `fin_wait_1`, `fin_wait_2`, `close_wait`, `closing`, `last_ack`, `time_wait`, `delete_tcb`.

#### Structs
```cpp
struct ip_address {
    address_family family = address_family::ipv4;
    std::array<unsigned char, 16> value {};
    std::uint32_t scope_id = 0;
};

struct socket_endpoint {
    ip_address address;
    std::uint16_t port = 0;
};

struct connection_entry {
    protocol transport_protocol = protocol::tcp;
    socket_endpoint local_endpoint;
    std::optional<socket_endpoint> remote_endpoint;
    tcp_state state = tcp_state::unknown;
    std::optional<std::uint32_t> pid;
    std::optional<std::uint32_t> uid;
    std::optional<std::uint64_t> inode;
    std::optional<std::uint32_t> send_queue_bytes;
    std::optional<std::uint32_t> receive_queue_bytes;
};
```

### Functions

```cpp
result<std::vector<connection_entry>> connections();
result<std::vector<connection_entry>> tcp_connections();
result<std::vector<connection_entry>> udp_endpoints();
result<std::vector<connection_entry>> listening_endpoints();
result<std::vector<connection_entry>> find_connections_by_process(std::uint32_t pid);
```

---

## 3. Wi-Fi Wireless Networking (`<syscape/wifi.hpp>`)

- **Profile**: Hosted Full (C++17)
- **Namespace**: `namespace syscape::wifi`

### Types and Enums

#### Enums
- `enum class adapter_power_state`: `unknown`, `on`, `off`, `blocked`.
- `enum class wifi_standard`: `unknown`, `legacy_802_11`, `wifi_4`, `wifi_5`, `wifi_6`, `wifi_6e`, `wifi_7`.
- `enum class frequency_band`: `unknown`, `band_2_4_ghz`, `band_5_ghz`, `band_6_ghz`, `band_60_ghz`.
- `enum class security_type`: `unknown`, `open`, `wep`, `wpa_personal`, `wpa_enterprise`, `wpa2_personal`, `wpa2_enterprise`, `wpa3_personal`, `wpa3_enterprise`, `wpa3_owe`.
- `enum class operation_mode`: `unknown`, `station`, `access_point`, `ad_hoc`, `mesh`.
- `enum class connection_state`: `unknown`, `disconnected`, `connecting`, `authenticating`, `connected`.

#### Structs
```cpp
struct network_connection {
    std::string ssid;
    std::string bssid;
    std::optional<std::int16_t> signal_dbm;
    std::optional<std::uint8_t> signal_quality_percent;
    std::optional<std::uint32_t> frequency_mhz;
    std::optional<std::uint16_t> channel;
    frequency_band band = frequency_band::unknown;
    wifi_standard standard = wifi_standard::unknown;
    security_type security = security_type::unknown;
    std::optional<std::uint32_t> transmit_rate_mbps;
    std::optional<std::uint32_t> receive_rate_mbps;
};

struct adapter_info {
    std::string id;
    std::string name;
    std::optional<std::string> mac_address;
    adapter_power_state power_state = adapter_power_state::unknown;
    operation_mode mode = operation_mode::unknown;
    connection_state state = connection_state::unknown;
    std::optional<network_connection> connection;
    std::vector<wifi_standard> supported_standards;
    std::vector<frequency_band> supported_bands;
};

struct configured_network {
    std::string ssid;
    std::optional<std::string> adapter_id;
    security_type security = security_type::unknown;
    std::optional<bool> auto_connect;
    std::optional<bool> is_hidden;
};
```

### Functions

```cpp
result<std::vector<adapter_info>> adapters();
result<std::size_t> adapter_count();
result<adapter_info> default_adapter();
result<std::optional<network_connection>> current_connection(std::string_view adapter_id = {});
result<std::vector<configured_network>> configured_networks();
```

---

## 4. Bluetooth Wireless (`<syscape/bluetooth.hpp>`)

- **Profile**: Hosted Full (C++17)
- **Namespace**: `namespace syscape::bluetooth`

### Types and Enums

#### Enums
- `enum class adapter_power_state`: `unknown`, `on`, `off`, `blocked`.
- `enum class adapter_bus_type`: `unknown`, `built_in`, `usb`, `pci`, `uart`, `sdio`, `virtual_bus`.
- `enum class major_device_class`: `unknown`, `computer`, `phone`, `audio_video`, `peripheral`, `imaging`, `wearable`, `toy`, `health`, `network_access_point`, `miscellaneous`.

#### Structs
```cpp
struct adapter_info {
    std::string id;
    std::optional<std::string> name;
    std::optional<std::string> address;
    adapter_power_state power_state = adapter_power_state::unknown;
    bool is_discoverable = false;
    bool is_connectable = false;
    adapter_bus_type bus_type = adapter_bus_type::unknown;
    std::optional<std::uint16_t> manufacturer_id;
    std::optional<std::uint8_t> hci_version;
    std::optional<std::uint16_t> hci_revision;
    std::optional<std::uint8_t> lmp_version;
    std::optional<std::uint16_t> lmp_subversion;
};

struct device_info {
    std::string address;
    std::optional<std::string> name;
    major_device_class device_class = major_device_class::unknown;
    std::uint32_t class_of_device = 0U;
    bool is_paired = false;
    bool is_connected = false;
    bool is_trusted = false;
    bool is_blocked = false;
    std::optional<std::int16_t> rssi_dbm;
    std::optional<std::uint8_t> battery_percent;
};
```

### Functions

```cpp
result<std::vector<adapter_info>> adapters();
result<std::size_t> adapter_count();
result<adapter_info> default_adapter();
result<std::vector<device_info>> paired_devices();
result<std::vector<device_info>> connected_devices();
```
