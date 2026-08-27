#ifndef SYSCAPE_DETAIL_CONNECTION_LINUX_HPP
#define SYSCAPE_DETAIL_CONNECTION_LINUX_HPP

#include <algorithm>
#include <array>
#include <charconv>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>

#include <syscape/detail/connection/common.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace connection_backend {

namespace linux_impl {

using inode_pid_map_type =
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>>;

template <typename UInt>
inline bool parse_unsigned(
    const std::string& text,
    int base,
    UInt& value) noexcept {
    static_assert(std::is_unsigned<UInt>::value, "UInt must be unsigned");
    if (text.empty()) {
        return false;
    }
    const char* const first = text.data();
    const char* const last = first + text.size();
    const std::from_chars_result parsed = std::from_chars(first, last, value, base);
    return parsed.ec == std::errc() && parsed.ptr == last;
}

inline bool host_is_little_endian() noexcept {
    const std::uint16_t value = 1U;
    return *reinterpret_cast<const unsigned char*>(&value) == 1U;
}

inline void store_proc_address_word(
    std::uint32_t value,
    std::array<unsigned char, 16>& out,
    std::size_t offset) noexcept {
    if (host_is_little_endian()) {
        out[offset + 0U] = static_cast<unsigned char>(value & 0xFFU);
        out[offset + 1U] = static_cast<unsigned char>((value >> 8U) & 0xFFU);
        out[offset + 2U] = static_cast<unsigned char>((value >> 16U) & 0xFFU);
        out[offset + 3U] = static_cast<unsigned char>((value >> 24U) & 0xFFU);
    } else {
        out[offset + 0U] = static_cast<unsigned char>((value >> 24U) & 0xFFU);
        out[offset + 1U] = static_cast<unsigned char>((value >> 16U) & 0xFFU);
        out[offset + 2U] = static_cast<unsigned char>((value >> 8U) & 0xFFU);
        out[offset + 3U] = static_cast<unsigned char>(value & 0xFFU);
    }
}

/// Parses an 8-character hex string representing an IPv4 address in kernel byte order.
inline bool parse_hex_ipv4(const std::string& hex, std::array<unsigned char, 16>& out) noexcept {
    if (hex.size() != 8) {
        return false;
    }
    std::uint32_t val = 0;
    if (!parse_unsigned(hex, 16, val)) {
        return false;
    }
    out.fill(0);
    store_proc_address_word(val, out, 0U);
    return true;
}

/// Parses a 32-character hex string representing an IPv6 address from /proc/net/tcp6.
inline bool parse_hex_ipv6(const std::string& hex, std::array<unsigned char, 16>& out) noexcept {
    if (hex.size() != 32) {
        return false;
    }
    out.fill(0);
    for (std::size_t word = 0; word < 4; ++word) {
        const std::string word_text = hex.substr(word * 8, 8);
        std::uint32_t val = 0;
        if (!parse_unsigned(word_text, 16, val)) {
            return false;
        }
        store_proc_address_word(val, out, word * 4U);
    }
    return true;
}

/// Maps Linux kernel numeric TCP state to portable tcp_state.
inline connection_common::tcp_state map_linux_tcp_state(unsigned long st) noexcept {
    switch (st) {
    case 1: return connection_common::tcp_state::established;
    case 2: return connection_common::tcp_state::syn_sent;
    case 3: return connection_common::tcp_state::syn_recv;
    case 4: return connection_common::tcp_state::fin_wait1;
    case 5: return connection_common::tcp_state::fin_wait2;
    case 6: return connection_common::tcp_state::time_wait;
    case 7: return connection_common::tcp_state::closed;
    case 8: return connection_common::tcp_state::close_wait;
    case 9: return connection_common::tcp_state::last_ack;
    case 10: return connection_common::tcp_state::listen;
    case 11: return connection_common::tcp_state::closing;
    default: return connection_common::tcp_state::unknown;
    }
}

/// Splits an endpoint string "HEXADDR:HEXPORT" into address and port.
inline bool parse_endpoint(
    const std::string& text,
    connection_common::address_family family,
    connection_common::socket_endpoint_record& out) noexcept {
    const std::size_t colon = text.find(':');
    if (colon == std::string::npos) {
        return false;
    }
    const std::string addr_hex = text.substr(0, colon);
    const std::string port_hex = text.substr(colon + 1);

    out.address.family = family;
    out.address.scope_id = 0;
    if (family == connection_common::address_family::ipv4) {
        if (!parse_hex_ipv4(addr_hex, out.address.value)) {
            return false;
        }
    } else {
        if (!parse_hex_ipv6(addr_hex, out.address.value)) {
            return false;
        }
    }

    std::uint32_t port_val = 0;
    if (!parse_unsigned(port_hex, 16, port_val) || port_val > 65535U) {
        return false;
    }
    out.port = static_cast<std::uint16_t>(port_val);
    return true;
}

/// Returns true if an address is all zeros (unspecified).
inline bool is_unspecified_address(
    connection_common::address_family family,
    const std::array<unsigned char, 16>& val) noexcept {
    const std::size_t len = (family == connection_common::address_family::ipv4) ? 4U : 16U;
    for (std::size_t i = 0; i < len; ++i) {
        if (val[i] != 0) {
            return false;
        }
    }
    return true;
}

/// Scans /proc to map socket inodes to owning process IDs (PIDs).
inline inode_pid_map_type build_inode_pid_map() {
    inode_pid_map_type map;
    DIR* const proc_dir = ::opendir("/proc");
    if (proc_dir == nullptr) {
        return map;
    }

    struct dir_closer {
        DIR* d;
        ~dir_closer() { if (d) { ::closedir(d); } }
    } closer{proc_dir};

    struct dirent* entry = nullptr;
    while ((entry = ::readdir(proc_dir)) != nullptr) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') {
            continue;
        }
        const std::string pid_text(entry->d_name);
        std::uint32_t pid = 0;
        if (!parse_unsigned(pid_text, 10, pid) || pid == 0U) {
            continue;
        }

        const std::string fd_dir_path = std::string("/proc/") + entry->d_name + "/fd";
        DIR* const fd_dir = ::opendir(fd_dir_path.c_str());
        if (fd_dir == nullptr) {
            continue;
        }
        const dir_closer fd_closer{fd_dir};

        struct dirent* fd_entry = nullptr;
        while ((fd_entry = ::readdir(fd_dir)) != nullptr) {
            if (fd_entry->d_name[0] == '.') {
                continue;
            }
            const std::string link_path = fd_dir_path + "/" + fd_entry->d_name;
            char target[256];
            const ssize_t len = ::readlink(link_path.c_str(), target, sizeof(target) - 1);
            if (len <= 0) {
                continue;
            }
            target[len] = '\0';
            // Check for socket:[<inode>] format
            if (std::strncmp(target, "socket:[", 8) == 0 && len > 9 && target[len - 1] == ']') {
                target[len - 1] = '\0';
                const std::string inode_text(target + 8);
                std::uint64_t inode = 0;
                if (parse_unsigned(inode_text, 10, inode)) {
                    std::vector<std::uint32_t>& owners = map[inode];
                    if (std::find(owners.begin(), owners.end(), pid) == owners.end()) {
                        owners.push_back(pid);
                    }
                }
            }
        }
    }

    for (auto& item : map) {
        std::sort(item.second.begin(), item.second.end());
    }
    return map;
}

/// Parses a single line from /proc/net/tcp, tcp6, udp, or udp6.
inline bool parse_proc_net_line(
    const std::string& line,
    connection_common::protocol proto,
    connection_common::address_family family,
    const inode_pid_map_type& inode_pid_map,
    connection_common::connection_record& out) {
    out = connection_common::connection_record {};
    std::istringstream iss(line);
    std::string sl;
    std::string local_addr_str;
    std::string rem_addr_str;
    std::string state_str;
    std::string queues_str;
    std::string timer_str;
    std::string retrans_str;
    std::string uid_str;
    std::string timeout_str;
    std::string inode_str;

    if (!(iss >> sl >> local_addr_str >> rem_addr_str >> state_str >> queues_str >>
          timer_str >> retrans_str >> uid_str >> timeout_str >> inode_str)) {
        return false;
    }

    if (sl.empty() || sl.back() != ':') {
        return false;
    }

    out.transport_protocol = proto;

    // Parse local endpoint
    if (!parse_endpoint(local_addr_str, family, out.local_endpoint)) {
        return false;
    }

    // Parse remote endpoint
    connection_common::socket_endpoint_record remote;
    if (!parse_endpoint(rem_addr_str, family, remote)) {
        return false;
    }
    if (is_unspecified_address(family, remote.address.value) && remote.port == 0) {
        out.remote_endpoint = std::nullopt;
    } else {
        out.remote_endpoint = remote;
    }

    // Parse state
    std::uint32_t st_val = 0;
    if (!parse_unsigned(state_str, 16, st_val)) {
        return false;
    }
    out.state = proto == connection_common::protocol::tcp
                    ? map_linux_tcp_state(st_val)
                    : connection_common::tcp_state::unknown;

    // Parse tx_queue:rx_queue
    const std::size_t colon = queues_str.find(':');
    if (colon == std::string::npos || queues_str.find(':', colon + 1) != std::string::npos) {
        return false;
    }
    std::uint32_t tx_val = 0;
    std::uint32_t rx_val = 0;
    if (!parse_unsigned(queues_str.substr(0, colon), 16, tx_val) ||
        !parse_unsigned(queues_str.substr(colon + 1), 16, rx_val)) {
        return false;
    }
    out.send_queue_bytes = tx_val;
    out.receive_queue_bytes = rx_val;

    // Parse UID
    std::uint32_t uid_val = 0;
    if (!parse_unsigned(uid_str, 10, uid_val)) {
        return false;
    }
    out.uid = uid_val;

    // Parse Inode
    std::uint64_t inode_num = 0;
    if (!parse_unsigned(inode_str, 10, inode_num)) {
        return false;
    }
    out.inode = inode_num;
    const auto it = inode_pid_map.find(inode_num);
    if (it != inode_pid_map.end() && !it->second.empty()) {
        out.pid = it->second.front();
    }

    return true;
}

/// Reads and parses a procfs network socket table file.
inline void append_record_for_owners(
    connection_common::connection_record rec,
    const inode_pid_map_type& inode_pid_map,
    std::vector<connection_common::connection_record>& out_records) {
    if (rec.inode) {
        const auto owners = inode_pid_map.find(*rec.inode);
        if (owners != inode_pid_map.end() && !owners->second.empty()) {
            for (const std::uint32_t pid : owners->second) {
                rec.pid = pid;
                out_records.push_back(rec);
            }
            return;
        }
    }
    rec.pid = std::nullopt;
    out_records.push_back(std::move(rec));
}

inline result<bool> read_proc_net_file(
    const char* path,
    connection_common::protocol proto,
    connection_common::address_family family,
    const inode_pid_map_type& inode_pid_map,
    std::vector<connection_common::connection_record>& out_records) {
    errno = 0;
    std::ifstream file(path);
    if (!file.is_open()) {
        const int saved_errno = errno;
        if (saved_errno == ENOENT || saved_errno == ENOTDIR) {
            return false;
        }
        return fail(std::error_code(saved_errno != 0 ? saved_errno : EIO,
                                    std::generic_category()));
    }
    std::string line;
    // Skip header line
    if (!std::getline(file, line)) {
        return fail(errc::malformed_data);
    }
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        connection_common::connection_record rec;
        if (!parse_proc_net_line(line, proto, family, inode_pid_map, rec)) {
            return fail(errc::malformed_data);
        }
        append_record_for_owners(std::move(rec), inode_pid_map, out_records);
    }
    if (file.bad()) {
        return fail(std::error_code(errno != 0 ? errno : EIO,
                                    std::generic_category()));
    }
    return true;
}

} // namespace linux_impl

inline result<std::vector<connection_common::connection_record>> connections() {
    std::vector<connection_common::connection_record> records;
    const auto inode_pid_map = linux_impl::build_inode_pid_map();

    bool any_source_found = false;
    const struct source {
        const char* path;
        connection_common::protocol protocol;
        connection_common::address_family family;
    } sources[] = {
        {"/proc/net/tcp", connection_common::protocol::tcp,
         connection_common::address_family::ipv4},
        {"/proc/net/tcp6", connection_common::protocol::tcp,
         connection_common::address_family::ipv6},
        {"/proc/net/udp", connection_common::protocol::udp,
         connection_common::address_family::ipv4},
        {"/proc/net/udp6", connection_common::protocol::udp,
         connection_common::address_family::ipv6}
    };
    for (const source& item : sources) {
        const result<bool> read = linux_impl::read_proc_net_file(
            item.path, item.protocol, item.family, inode_pid_map, records);
        if (!read) {
            return fail(read.error());
        }
        any_source_found = any_source_found || *read;
    }

    if (!any_source_found) {
        return fail(make_error_code(errc::not_supported));
    }

    connection_common::sort_connection_records(records);
    return records;
}

} // namespace connection_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_CONNECTION_LINUX_HPP
