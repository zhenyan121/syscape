#ifndef SYSCAPE_DETAIL_WIFI_LINUX_HPP
#define SYSCAPE_DETAIL_WIFI_LINUX_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <limits>
#include <net/if.h>
#include <optional>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <syscape/wifi.hpp>
#include <syscape/detail/linux/directory.hpp>
#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/detail/wifi/common.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

// Standard Wireless Extensions (WEXT) ioctl commands.
#ifndef SIOCGIWNAME
#define SIOCGIWNAME 0x8B01
#endif
#ifndef SIOCGIWFREQ
#define SIOCGIWFREQ 0x8B05
#endif
#ifndef SIOCGIWMODE
#define SIOCGIWMODE 0x8B07
#endif
#ifndef SIOCGIWAP
#define SIOCGIWAP 0x8B15
#endif
#ifndef SIOCGIWESSID
#define SIOCGIWESSID 0x8B1B
#endif
#ifndef SIOCGIWRATE
#define SIOCGIWRATE 0x8B21
#endif

#ifndef IW_MODE_AUTO
#define IW_MODE_AUTO 0
#define IW_MODE_ADHOC 1
#define IW_MODE_INFRA 2
#define IW_MODE_MASTER 3
#define IW_MODE_REPEAT 4
#define IW_MODE_SECOND 5
#define IW_MODE_MONITOR 6
#define IW_MODE_MESH 7
#endif

namespace syscape {
namespace detail {
namespace wifi_backend {

struct wext_freq {
    std::int32_t m;
    std::int16_t e;
    std::uint8_t i;
    std::uint8_t flags;
};

struct wext_point {
    void* pointer;
    std::uint16_t length;
    std::uint16_t flags;
};

struct wext_param {
    std::int32_t value;
    std::uint8_t fixed;
    std::uint8_t disabled;
    std::uint16_t flags;
};

inline bool is_associated_bssid(const std::uint8_t* bytes) noexcept {
    bool all_zero = true;
    bool all_ff = true;
    for (std::size_t index = 0U; index < 6U; ++index) {
        all_zero = all_zero && bytes[index] == 0x00U;
        all_ff = all_ff && bytes[index] == 0xFFU;
    }
    return !all_zero && !all_ff;
}

inline void apply_wext_frequency(const wext_freq& frequency,
                                 wifi::network_connection& connection) noexcept {
    // Wireless Extensions permits drivers to return a channel number in m
    // with e == 0. Such a value is not a frequency in hertz.
    if (frequency.e == 0 && frequency.m > 0 && frequency.m <= 233) {
        connection.channel = static_cast<std::uint16_t>(frequency.m);
        return;
    }

    long double frequency_hz = static_cast<long double>(frequency.m);
    if (frequency.e > 0) {
        for (int index = 0; index < frequency.e; ++index) {
            frequency_hz *= 10.0L;
        }
    } else {
        for (int index = 0; index > frequency.e; --index) {
            frequency_hz /= 10.0L;
        }
    }
    const long double frequency_mhz_value = frequency_hz / 1000000.0L;
    if (frequency_mhz_value < 1.0L ||
        frequency_mhz_value > static_cast<long double>(
            (std::numeric_limits<std::uint32_t>::max)())) {
        return;
    }
    const auto frequency_mhz =
        static_cast<std::uint32_t>(frequency_mhz_value);
    const auto band = wifi_common::frequency_to_band(frequency_mhz);
    if (band == wifi::frequency_band::unknown) {
        return;
    }
    connection.frequency_mhz = frequency_mhz;
    connection.band = band;
    connection.channel = wifi_common::frequency_to_channel(frequency_mhz);
}

union wext_data {
    char name[16];
    wext_point essid;
    wext_freq freq;
    wext_param bitrate;
    struct sockaddr ap_addr;
    std::uint32_t mode;
    char padding[128];
};

struct wext_req {
    union {
        char ifrn_name[16];
    } ifr_ifrn;
    wext_data u;
};

struct wireless_proc_entry {
    std::string interface_name;
    std::uint16_t status = 0;
    std::optional<std::uint8_t> link_quality;
    std::optional<std::int16_t> signal_dbm;
    std::optional<std::int16_t> noise_dbm;
};

inline result<const ::dirent*> next_directory_entry(::DIR* directory) {
    errno = 0;
    const auto* entry = ::readdir(directory);
    if (entry == nullptr && errno != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    return entry;
}

inline result<std::optional<std::string>>
read_sysfs_string(const std::string& path) {
    auto content = linux_platform::read_text_file(path.c_str(), 4096U);
    if (!content) {
        if (content.error() == std::errc::no_such_file_or_directory ||
            content.error() == std::errc::not_a_directory) {
            return std::optional<std::string>{};
        }
        return fail(content.error());
    }
    linux_platform::trim_line_end(*content);
    std::string_view trimmed = wifi_common::trim_whitespace(*content);
    if (trimmed.empty()) {
        return std::optional<std::string>{};
    }
    if (!is_valid_utf8(trimmed)) {
        return fail(errc::invalid_encoding);
    }
    return std::optional<std::string>{std::string(trimmed)};
}

inline bool is_wireless_interface(const std::string& ifname) {
    const std::string net_dir = "/sys/class/net/" + ifname;
    struct ::stat st{};
    if (::stat((net_dir + "/wireless").c_str(), &st) == 0 &&
        S_ISDIR(st.st_mode)) {
        return true;
    }
    if (::stat((net_dir + "/phy80211").c_str(), &st) == 0) {
        return true;
    }
    return false;
}

inline result<wifi::adapter_power_state>
read_rfkill_directory(const std::string& directory_path) {
    linux_platform::directory_handle directory(directory_path.c_str());
    if (!directory.valid()) {
        if (directory.error() == ENOENT || directory.error() == ENOTDIR) {
            return wifi::adapter_power_state::unknown;
        }
        return fail(std::error_code(directory.error(), std::generic_category()));
    }

    while (true) {
        const auto entry = next_directory_entry(directory.get());
        if (!entry) {
            return fail(entry.error());
        }
        if (*entry == nullptr) {
            break;
        }
        const std::string_view name((*entry)->d_name);
        if (name.size() < 6U || name.substr(0U, 6U) != "rfkill") {
            continue;
        }
        const std::string path = directory_path + "/" + std::string(name);
        const auto hard = read_sysfs_string(path + "/hard");
        const auto soft = read_sysfs_string(path + "/soft");
        if (!hard) {
            return fail(hard.error());
        }
        if (!soft) {
            return fail(soft.error());
        }
        if ((hard->has_value() && **hard == "1") ||
            (soft->has_value() && **soft == "1")) {
            return wifi::adapter_power_state::blocked;
        }
    }
    return wifi::adapter_power_state::unknown;
}

inline result<wifi::adapter_power_state>
query_rfkill_state(const std::string& ifname,
                   const std::string& net_root = "/sys/class/net") {
    const std::string net_dir = net_root + "/" + ifname;
    const auto direct = read_rfkill_directory(net_dir);
    if (!direct || *direct != wifi::adapter_power_state::unknown) {
        return direct;
    }
    return read_rfkill_directory(net_dir + "/phy80211");
}

inline std::vector<wireless_proc_entry>
parse_proc_net_wireless_text(std::string_view content) {
    std::vector<wireless_proc_entry> entries;
    std::size_t line_start = 0;
    std::size_t line_index = 0;

    while (line_start < content.size()) {
        std::size_t line_end = content.find('\n', line_start);
        if (line_end == std::string_view::npos) {
            line_end = content.size();
        }
        std::string_view line =
            content.substr(line_start, line_end - line_start);
        line_start = line_end + 1;
        ++line_index;

        if (line_index <= 2U) {
            continue; // Skip 2 header lines
        }

        line = wifi_common::trim_whitespace(line);
        if (line.empty()) {
            continue;
        }

        const std::size_t colon_pos = line.find(':');
        if (colon_pos == std::string_view::npos) {
            continue;
        }

        wireless_proc_entry entry;
        entry.interface_name =
            std::string(wifi_common::trim_whitespace(line.substr(0, colon_pos)));

        std::string_view rest =
            wifi_common::trim_whitespace(line.substr(colon_pos + 1));
        std::vector<std::string_view> tokens;
        std::size_t tok_pos = 0;
        while (tok_pos < rest.size()) {
            while (tok_pos < rest.size() &&
                   static_cast<unsigned char>(rest[tok_pos]) <= ' ') {
                ++tok_pos;
            }
            if (tok_pos >= rest.size()) {
                break;
            }
            std::size_t tok_end = tok_pos;
            while (tok_end < rest.size() &&
                   static_cast<unsigned char>(rest[tok_end]) > ' ') {
                ++tok_end;
            }
            tokens.push_back(rest.substr(tok_pos, tok_end - tok_pos));
            tok_pos = tok_end;
        }

        if (!tokens.empty()) {
            const auto st =
                wifi_common::parse_int<std::uint16_t>(tokens[0], 16);
            if (st) {
                entry.status = *st;
            }
        }
        if (tokens.size() >= 2U) {
            std::string_view q_tok = tokens[1];
            while (!q_tok.empty() && q_tok.back() == '.') {
                q_tok.remove_suffix(1);
            }
            const auto q = wifi_common::parse_int<std::uint8_t>(q_tok, 10);
            if (q) {
                entry.link_quality = *q;
            }
        }
        if (tokens.size() >= 3U) {
            std::string_view s_tok = tokens[2];
            while (!s_tok.empty() && s_tok.back() == '.') {
                s_tok.remove_suffix(1);
            }
            const auto sig = wifi_common::parse_signed_int<std::int16_t>(s_tok);
            if (sig) {
                entry.signal_dbm = *sig;
            }
        }
        if (tokens.size() >= 4U) {
            std::string_view n_tok = tokens[3];
            while (!n_tok.empty() && n_tok.back() == '.') {
                n_tok.remove_suffix(1);
            }
            const auto noise =
                wifi_common::parse_signed_int<std::int16_t>(n_tok);
            if (noise) {
                entry.noise_dbm = *noise;
            }
        }

        entries.push_back(entry);
    }

    return entries;
}

inline std::vector<wireless_proc_entry> read_proc_net_wireless() {
    const auto content = linux_platform::read_text_file("/proc/net/wireless", 4096U);
    if (!content) {
        return {};
    }
    return parse_proc_net_wireless_text(*content);
}

class socket_guard {
public:
    socket_guard() noexcept
        : fd_(::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0)),
          error_(fd_ < 0 ? errno : 0) {}
    ~socket_guard() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    socket_guard(const socket_guard&) = delete;
    socket_guard& operator=(const socket_guard&) = delete;

    int get() const noexcept { return fd_; }
    bool valid() const noexcept { return fd_ >= 0; }
    int error() const noexcept { return error_; }

private:
    int fd_;
    int error_;
};

inline result<std::optional<wifi::network_connection>>
query_wext_connection(int sock, const std::string& ifname) {
    if (sock < 0 || ifname.empty() || ifname.size() >= IFNAMSIZ) {
        return fail(errc::invalid_argument);
    }

    // 1. Query AP BSSID. WEXT can retain a configured ESSID after an
    // interface becomes unassociated, so a valid access-point address is the
    // authority for whether an active connection exists.
    wext_req wrq_ap{};
    std::strncpy(wrq_ap.ifr_ifrn.ifrn_name, ifname.c_str(), IFNAMSIZ - 1);
    if (::ioctl(sock, SIOCGIWAP, &wrq_ap) < 0) {
        return std::optional<wifi::network_connection>{};
    }
    const auto* raw_bssid =
        reinterpret_cast<const std::uint8_t*>(wrq_ap.u.ap_addr.sa_data);
    if (!is_associated_bssid(raw_bssid)) {
        return std::optional<wifi::network_connection>{};
    }

    // 2. Query ESSID only after association has been established. The flags
    // field indicates whether the returned ESSID is active.
    char essid_buf[33] = {0};
    wext_req wrq_essid{};
    std::strncpy(wrq_essid.ifr_ifrn.ifrn_name, ifname.c_str(), IFNAMSIZ - 1);
    wrq_essid.u.essid.pointer = essid_buf;
    wrq_essid.u.essid.length = sizeof(essid_buf);
    wrq_essid.u.essid.flags = 0;

    std::string ssid;
    if (::ioctl(sock, SIOCGIWESSID, &wrq_essid) >= 0 &&
        wrq_essid.u.essid.flags != 0U) {
        essid_buf[sizeof(essid_buf) - 1] = '\0';
        const std::size_t ssid_size = (std::min)(
            static_cast<std::size_t>(wrq_essid.u.essid.length),
            sizeof(essid_buf) - 1U);
        std::string_view raw_ssid(essid_buf, ssid_size);
        while (!raw_ssid.empty() && raw_ssid.back() == '\0') {
            raw_ssid.remove_suffix(1U);
        }
        if (!is_valid_utf8(raw_ssid)) {
            return fail(errc::invalid_encoding);
        }
        ssid = std::string(raw_ssid);
    }

    wifi::network_connection conn;
    conn.ssid = std::move(ssid);
    conn.bssid = wifi_common::format_mac_bytes(raw_bssid);

    // 3. Query Frequency
    wext_req wrq_freq{};
    std::strncpy(wrq_freq.ifr_ifrn.ifrn_name, ifname.c_str(), IFNAMSIZ - 1);
    if (::ioctl(sock, SIOCGIWFREQ, &wrq_freq) >= 0) {
        apply_wext_frequency(wrq_freq.u.freq, conn);
    }

    // 4. Query Bitrate
    wext_req wrq_rate{};
    std::strncpy(wrq_rate.ifr_ifrn.ifrn_name, ifname.c_str(), IFNAMSIZ - 1);
    if (::ioctl(sock, SIOCGIWRATE, &wrq_rate) >= 0 &&
        wrq_rate.u.bitrate.value > 0) {
        conn.transmit_rate_mbps = static_cast<std::uint32_t>(
            wrq_rate.u.bitrate.value / 1000000);
    }

    // 5. Query Protocol Name for standard
    wext_req wrq_name{};
    std::strncpy(wrq_name.ifr_ifrn.ifrn_name, ifname.c_str(), IFNAMSIZ - 1);
    if (::ioctl(sock, SIOCGIWNAME, &wrq_name) >= 0) {
        wrq_name.u.name[15] = '\0';
        std::string_view proto_name(wrq_name.u.name);
        if (proto_name.find("802.11be") != std::string_view::npos ||
            proto_name.find("WiFi 7") != std::string_view::npos) {
            conn.standard = wifi::wifi_standard::wifi_7;
        } else if (proto_name.find("802.11ax") != std::string_view::npos ||
                   proto_name.find("WiFi 6") != std::string_view::npos) {
            if (conn.band == wifi::frequency_band::band_6_ghz) {
                conn.standard = wifi::wifi_standard::wifi_6e;
            } else {
                conn.standard = wifi::wifi_standard::wifi_6;
            }
        } else if (proto_name.find("802.11ac") != std::string_view::npos ||
                   proto_name.find("WiFi 5") != std::string_view::npos) {
            conn.standard = wifi::wifi_standard::wifi_5;
        } else if (proto_name.find("802.11n") != std::string_view::npos ||
                   proto_name.find("WiFi 4") != std::string_view::npos) {
            conn.standard = wifi::wifi_standard::wifi_4;
        } else if (proto_name.find("802.11") != std::string_view::npos) {
            conn.standard = wifi::wifi_standard::legacy_802_11;
        }
    }

    return std::optional<wifi::network_connection>{std::move(conn)};
}

inline wifi::operation_mode query_wext_mode(int sock, const std::string& ifname) {
    if (sock < 0 || ifname.empty() || ifname.size() >= IFNAMSIZ) {
        return wifi::operation_mode::unknown;
    }
    wext_req wrq_mode{};
    std::strncpy(wrq_mode.ifr_ifrn.ifrn_name, ifname.c_str(), IFNAMSIZ - 1);
    if (::ioctl(sock, SIOCGIWMODE, &wrq_mode) >= 0) {
        switch (wrq_mode.u.mode) {
        case IW_MODE_INFRA:
            return wifi::operation_mode::station;
        case IW_MODE_MASTER:
            return wifi::operation_mode::access_point;
        case IW_MODE_ADHOC:
            return wifi::operation_mode::ad_hoc;
        case IW_MODE_MESH:
            return wifi::operation_mode::mesh;
        default:
            return wifi::operation_mode::unknown;
        }
    }
    return wifi::operation_mode::unknown;
}

inline result<std::vector<wifi::adapter_info>> adapters() {
    linux_platform::directory_handle dir("/sys/class/net");
    if (!dir.valid()) {
        return fail(std::error_code(dir.error(), std::generic_category()));
    }

    std::vector<std::string> ifnames;
    while (true) {
        const auto entry = next_directory_entry(dir.get());
        if (!entry) {
            return fail(entry.error());
        }
        if (*entry == nullptr) {
            break;
        }
        const std::string_view name((*entry)->d_name);
        if (name == "." || name == "..") {
            continue;
        }
        if (is_wireless_interface(std::string(name))) {
            ifnames.emplace_back(name);
        }
    }

    std::sort(ifnames.begin(), ifnames.end(), wifi_common::natural_less);

    socket_guard sock;
    const auto proc_wireless = read_proc_net_wireless();

    std::vector<wifi::adapter_info> result_list;
    result_list.reserve(ifnames.size());

    for (const auto& ifname : ifnames) {
        wifi::adapter_info info;
        info.id = ifname;

        const std::string net_dir = "/sys/class/net/" + ifname;

        // Linux does not expose a stable friendly name in sysfs. The
        // interface name is the honest portable display value.
        info.name = ifname;

        // MAC address
        const auto mac_str = read_sysfs_string(net_dir + "/address");
        if (!mac_str) {
            return fail(mac_str.error());
        }
        if (mac_str->has_value()) {
            info.mac_address = wifi_common::normalize_mac_address(**mac_str);
            if (!info.mac_address) {
                return fail(errc::malformed_data);
            }
        }

        // Operational state. A sysfs carrier state cannot establish radio
        // power, and only an associated BSSID/SSID establishes connection.
        const auto oper_str = read_sysfs_string(net_dir + "/operstate");
        if (!oper_str) {
            return fail(oper_str.error());
        }
        if (oper_str->has_value()) {
            if (**oper_str == "down") {
                info.state = wifi::connection_state::disconnected;
            } else if (**oper_str == "dormant" || **oper_str == "testing") {
                info.state = wifi::connection_state::connecting;
            }
        }

        // Radio power state
        const auto rfkill_state = query_rfkill_state(ifname);
        if (!rfkill_state) {
            return fail(rfkill_state.error());
        }
        info.power_state = *rfkill_state;

        // Operational mode
        if (sock.valid()) {
            info.mode = query_wext_mode(sock.get(), ifname);
        }

        // Active connection details
        if (sock.valid()) {
            const auto conn_result = query_wext_connection(sock.get(), ifname);
            if (!conn_result) {
                return fail(conn_result.error());
            }
            auto conn = *conn_result;
            if (conn.has_value()) {
                // Enrich with /proc/net/wireless signal info if available
                for (const auto& pentry : proc_wireless) {
                    if (pentry.interface_name == ifname) {
                        if (pentry.signal_dbm) {
                            const std::int16_t signal = *pentry.signal_dbm;
                            if (signal >= -127 && signal <= 0) {
                                conn->signal_dbm = signal;
                                conn->signal_quality_percent =
                                    wifi_common::rssi_to_quality_percent(signal);
                            }
                        }
                        break;
                    }
                }
                info.connection = std::move(conn);
                info.state = wifi::connection_state::connected;
                if (info.power_state == wifi::adapter_power_state::unknown) {
                    info.power_state = wifi::adapter_power_state::on;
                }
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
    socket_guard sock;
    if (!sock.valid()) {
        return fail(std::error_code(sock.error(), std::generic_category()));
    }

    std::string target_id;
    if (adapter_id.empty()) {
        const auto def = default_adapter();
        if (!def) {
            return fail(def.error());
        }
        target_id = def->id;
    } else {
        target_id = std::string(adapter_id);
    }

    if (!is_wireless_interface(target_id)) {
        return fail(errc::not_found);
    }

    const auto conn_result = query_wext_connection(sock.get(), target_id);
    if (!conn_result) {
        return fail(conn_result.error());
    }
    auto conn = *conn_result;
    if (!conn.has_value()) {
        return conn;
    }

    const auto proc_wireless = read_proc_net_wireless();
    for (const auto& pentry : proc_wireless) {
        if (pentry.interface_name == target_id) {
            if (pentry.signal_dbm) {
                const std::int16_t signal = *pentry.signal_dbm;
                if (signal >= -127 && signal <= 0) {
                    conn->signal_dbm = signal;
                    conn->signal_quality_percent =
                        wifi_common::rssi_to_quality_percent(signal);
                }
            }
            break;
        }
    }

    return conn;
}

inline result<std::string>
decode_network_manager_string(std::string_view value) {
    if (!value.empty() && value.back() == ';') {
        std::string bytes;
        std::size_t begin = 0U;
        bool decimal_list = true;
        while (begin < value.size()) {
            const std::size_t end = value.find(';', begin);
            if (end == std::string_view::npos) {
                decimal_list = false;
                break;
            }
            const std::string_view token =
                wifi_common::trim_whitespace(value.substr(begin, end - begin));
            if (!token.empty()) {
                const auto byte = wifi_common::parse_int<std::uint8_t>(token);
                if (!byte) {
                    decimal_list = false;
                    break;
                }
                bytes.push_back(static_cast<char>(*byte));
            }
            begin = end + 1U;
        }
        if (decimal_list) {
            if (!is_valid_utf8(bytes)) {
                return fail(errc::invalid_encoding);
            }
            return bytes;
        }
    }

    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t index = 0U; index < value.size(); ++index) {
        if (value[index] != '\\') {
            decoded.push_back(value[index]);
            continue;
        }
        if (++index >= value.size()) {
            return fail(errc::malformed_data);
        }
        switch (value[index]) {
        case 's': decoded.push_back(' '); break;
        case 'n': decoded.push_back('\n'); break;
        case 't': decoded.push_back('\t'); break;
        case 'r': decoded.push_back('\r'); break;
        case '\\': decoded.push_back('\\'); break;
        case ';': decoded.push_back(';'); break;
        case ',': decoded.push_back(','); break;
        default: return fail(errc::malformed_data);
        }
    }
    if (!is_valid_utf8(decoded)) {
        return fail(errc::invalid_encoding);
    }
    return decoded;
}

inline result<bool> parse_network_manager_bool(std::string_view value) {
    if (value == "true" || value == "yes" || value == "on" || value == "1") {
        return true;
    }
    if (value == "false" || value == "no" || value == "off" || value == "0") {
        return false;
    }
    return fail(errc::malformed_data);
}

inline result<std::optional<wifi::configured_network>>
parse_network_manager_profile(std::string_view content) {
    std::size_t line_start = 0;
    std::string current_ssid;
    wifi::security_type current_sec = wifi::security_type::unknown;
    std::optional<bool> autoconnect;
    std::optional<bool> hidden;
    std::string current_section;
    bool has_wifi_security = false;
    bool has_wep_key = false;
    std::string key_management;
    bool allows_wpa = false;
    bool allows_rsn = false;

    while (line_start < content.size()) {
        std::size_t line_end = content.find('\n', line_start);
        if (line_end == std::string_view::npos) {
            line_end = content.size();
        }
        std::string_view line =
            wifi_common::trim_whitespace(content.substr(line_start, line_end - line_start));
        line_start = line_end + 1;

        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            current_section = std::string(line.substr(1, line.size() - 2));
            continue;
        }

        const std::size_t eq_pos = line.find('=');
        if (eq_pos == std::string_view::npos) {
            continue;
        }

        const std::string_view key =
            wifi_common::trim_whitespace(line.substr(0, eq_pos));
        const std::string_view val =
            wifi_common::trim_whitespace(line.substr(eq_pos + 1));

        if (key == "ssid" && (current_section == "wifi" || current_section == "802-11-wireless")) {
            const auto decoded_ssid = decode_network_manager_string(val);
            if (!decoded_ssid) {
                return fail(decoded_ssid.error());
            }
            current_ssid = *decoded_ssid;
        } else if ((key == "key-mgmt" || key == "key_mgmt") &&
                   (current_section == "wifi-security" ||
                    current_section == "802-11-wireless-security")) {
            has_wifi_security = true;
            key_management = std::string(val);
        } else if (key == "proto" &&
                   (current_section == "wifi-security" ||
                    current_section == "802-11-wireless-security")) {
            allows_wpa = val.find("wpa") != std::string_view::npos;
            allows_rsn = val.find("rsn") != std::string_view::npos;
        } else if (key.size() == 8U && key.substr(0U, 7U) == "wep-key" &&
                   key[7] >= '0' && key[7] <= '3' &&
                   (current_section == "wifi-security" ||
                    current_section == "802-11-wireless-security")) {
            has_wep_key = true;
        } else if (key == "autoconnect" && current_section == "connection") {
            const auto value = parse_network_manager_bool(val);
            if (!value) {
                return fail(value.error());
            }
            autoconnect = *value;
        } else if (key == "hidden" &&
                   (current_section == "wifi" ||
                    current_section == "802-11-wireless")) {
            const auto value = parse_network_manager_bool(val);
            if (!value) {
                return fail(value.error());
            }
            hidden = *value;
        }
    }

    if (current_ssid.empty()) {
        return std::optional<wifi::configured_network>{};
    }
    wifi::configured_network net;
    net.ssid = std::move(current_ssid);
    if (!has_wifi_security) {
        current_sec = wifi::security_type::open;
    } else if (key_management == "sae" || key_management == "SAE") {
        current_sec = wifi::security_type::wpa3_personal;
    } else if (key_management == "owe" || key_management == "OWE") {
        current_sec = wifi::security_type::wpa3_owe;
    } else if (key_management == "wpa-eap-suite-b-192" ||
               key_management == "WPA-EAP-SUITE-B-192") {
        current_sec = wifi::security_type::wpa3_enterprise;
    } else if (key_management == "none" || key_management == "NONE") {
        current_sec = has_wep_key ? wifi::security_type::wep
                                  : wifi::security_type::open;
    } else if (key_management == "ieee8021x" ||
               key_management == "IEEE8021X") {
        current_sec = wifi::security_type::wep;
    } else if (key_management == "wpa-psk" || key_management == "WPA-PSK") {
        if (allows_wpa != allows_rsn) {
            current_sec = allows_wpa ? wifi::security_type::wpa_personal
                                     : wifi::security_type::wpa2_personal;
        }
    } else if (key_management == "wpa-eap" || key_management == "WPA-EAP") {
        if (allows_wpa != allows_rsn) {
            current_sec = allows_wpa ? wifi::security_type::wpa_enterprise
                                     : wifi::security_type::wpa2_enterprise;
        }
    }
    net.security = current_sec;
    net.auto_connect = autoconnect;
    net.is_hidden = hidden;
    return std::optional<wifi::configured_network>{std::move(net)};
}

inline result<std::string> decode_iwd_ssid(std::string_view encoded) {
    if (!encoded.empty() && encoded.front() == '=') {
        encoded.remove_prefix(1U);
        if (encoded.empty() || encoded.size() % 2U != 0U) {
            return fail(errc::malformed_data);
        }
        std::string decoded;
        decoded.reserve(encoded.size() / 2U);
        for (std::size_t index = 0U; index < encoded.size(); index += 2U) {
            const auto value = wifi_common::parse_int<std::uint8_t>(
                encoded.substr(index, 2U), 16);
            if (!value) {
                return fail(errc::malformed_data);
            }
            decoded.push_back(static_cast<char>(*value));
        }
        if (!is_valid_utf8(decoded)) {
            return fail(errc::invalid_encoding);
        }
        return decoded;
    }
    if (!is_valid_utf8(encoded)) {
        return fail(errc::invalid_encoding);
    }
    return std::string(encoded);
}

inline result<std::vector<wifi::configured_network>> configured_networks() {
    std::vector<wifi::configured_network> profiles;

    // 1. Try NetworkManager system connections
    const char* const nm_dir_path = "/etc/NetworkManager/system-connections";
    linux_platform::directory_handle nm_dir(nm_dir_path);
    if (!nm_dir.valid() && nm_dir.error() != ENOENT &&
        nm_dir.error() != ENOTDIR) {
        return fail(std::error_code(nm_dir.error(), std::generic_category()));
    }
    if (nm_dir.valid()) {
        while (true) {
            const auto entry = next_directory_entry(nm_dir.get());
            if (!entry) {
                return fail(entry.error());
            }
            if (*entry == nullptr) {
                break;
            }
            const std::string_view name((*entry)->d_name);
            if (name == "." || name == "..") {
                continue;
            }
            const std::string file_path = std::string(nm_dir_path) + "/" + std::string(name);
            const auto file_content = linux_platform::read_text_file(file_path.c_str(), 16384U);
            if (!file_content) {
                return fail(file_content.error());
            }
            const auto profile = parse_network_manager_profile(*file_content);
            if (!profile) {
                return fail(profile.error());
            }
            if (profile->has_value()) {
                profiles.push_back(**profile);
            }
        }
    }

    // 2. Try iwd profiles (/var/lib/iwd)
    const char* const iwd_dir_path = "/var/lib/iwd";
    linux_platform::directory_handle iwd_dir(iwd_dir_path);
    if (!iwd_dir.valid() && iwd_dir.error() != ENOENT &&
        iwd_dir.error() != ENOTDIR) {
        return fail(std::error_code(iwd_dir.error(), std::generic_category()));
    }
    if (iwd_dir.valid()) {
        while (true) {
            const auto entry = next_directory_entry(iwd_dir.get());
            if (!entry) {
                return fail(entry.error());
            }
            if (*entry == nullptr) {
                break;
            }
            const std::string_view name((*entry)->d_name);
            if (name == "." || name == "..") {
                continue;
            }
            std::size_t suffix_size = 0U;
            wifi::security_type security = wifi::security_type::unknown;
            if (name.size() > 4U && name.substr(name.size() - 4U) == ".psk") {
                suffix_size = 4U;
                // The filename does not distinguish WPA2-PSK from WPA3-SAE.
                security = wifi::security_type::unknown;
            } else if (name.size() > 5U &&
                       name.substr(name.size() - 5U) == ".open") {
                suffix_size = 5U;
                security = wifi::security_type::open;
            } else if (name.size() > 6U &&
                       name.substr(name.size() - 6U) == ".8021x") {
                suffix_size = 6U;
                // The suffix establishes 802.1X, not a WPA generation.
                security = wifi::security_type::unknown;
            }
            if (suffix_size != 0U) {
                const auto ssid = decode_iwd_ssid(
                    name.substr(0U, name.size() - suffix_size));
                if (!ssid) {
                    return fail(ssid.error());
                }
                wifi::configured_network net;
                net.ssid = *ssid;
                net.security = security;
                profiles.push_back(std::move(net));
            }
        }
    }

    return profiles;
}

} // namespace wifi_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_WIFI_LINUX_HPP
