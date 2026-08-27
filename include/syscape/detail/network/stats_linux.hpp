#ifndef SYSCAPE_DETAIL_NETWORK_STATS_LINUX_HPP
#define SYSCAPE_DETAIL_NETWORK_STATS_LINUX_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <net/if.h>
#include <sys/types.h>

#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/network/common.hpp>
#include <syscape/detail/network/posix.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace network_backend {

/// Parses an unsigned 64-bit integer from ASCII text with overflow detection.
inline result<std::uint64_t> parse_uint64_exact(std::string_view text) noexcept {
    if (text.empty()) { return fail(errc::malformed_data); }
    std::uint64_t value = 0U;
    for (char c : text) {
        if (c < '0' || c > '9') { return fail(errc::malformed_data); }
        const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
        if (value > (UINT64_MAX - digit) / 10U) {
            return fail(errc::value_too_large);
        }
        value = value * 10U + digit;
    }
    return value;
}

/// Parses the contents of /proc/net/dev into statistics records.
///
/// InterfaceApi resolves interface names to OS indices.
template <typename InterfaceApi = native_interface_api>
inline result<std::vector<network_common::statistics_record>> parse_proc_net_dev(
    std::string_view content) {
    std::vector<network_common::statistics_record> records;
    std::size_t pos = 0U;

    while (pos < content.size()) {
        const std::size_t line_end = content.find('\n', pos);
        const std::size_t line_len = (line_end == std::string_view::npos)
                                         ? content.size() - pos
                                         : line_end - pos;
        std::string_view line = content.substr(pos, line_len);
        pos = (line_end == std::string_view::npos) ? content.size()
                                                   : line_end + 1U;

        const std::size_t colon_pos = line.rfind(':');
        if (colon_pos == std::string_view::npos) {
            std::string_view trimmed = line;
            while (!trimmed.empty() &&
                   (trimmed.front() == ' ' || trimmed.front() == '\t')) {
                trimmed.remove_prefix(1U);
            }
            if (trimmed.empty() ||
                trimmed.find("Inter-|") == 0U ||
                trimmed.find("face |") == 0U) {
                continue;
            }
            return fail(errc::malformed_data);
        }

        std::string_view name_part = line.substr(0U, colon_pos);
        while (!name_part.empty() &&
               (name_part.front() == ' ' || name_part.front() == '\t')) {
            name_part.remove_prefix(1U);
        }
        while (!name_part.empty() &&
               (name_part.back() == ' ' || name_part.back() == '\t' ||
                name_part.back() == '\r')) {
            name_part.remove_suffix(1U);
        }

        if (name_part.empty()) {
            return fail(errc::malformed_data);
        }
        if (!is_valid_utf8(name_part)) {
            return fail(errc::invalid_encoding);
        }

        std::string_view numbers_part = line.substr(colon_pos + 1U);
        std::uint64_t fields[16] = {0U};
        std::size_t field_idx = 0U;
        std::size_t num_pos = 0U;

        while (num_pos < numbers_part.size() && field_idx < 16U) {
            while (num_pos < numbers_part.size() &&
                   (numbers_part[num_pos] == ' ' ||
                    numbers_part[num_pos] == '\t' ||
                    numbers_part[num_pos] == '\r')) {
                ++num_pos;
            }
            if (num_pos >= numbers_part.size()) {
                break;
            }
            const std::size_t token_start = num_pos;
            while (num_pos < numbers_part.size() &&
                   numbers_part[num_pos] != ' ' &&
                   numbers_part[num_pos] != '\t' &&
                   numbers_part[num_pos] != '\r') {
                ++num_pos;
            }
            std::string_view token =
                numbers_part.substr(token_start, num_pos - token_start);
            const result<std::uint64_t> parsed = parse_uint64_exact(token);
            if (!parsed) { return fail(parsed.error()); }
            fields[field_idx++] = *parsed;
        }

        if (field_idx < 16U) {
            return fail(errc::malformed_data);
        }
        while (num_pos < numbers_part.size() &&
               (numbers_part[num_pos] == ' ' ||
                numbers_part[num_pos] == '\t' ||
                numbers_part[num_pos] == '\r')) {
            ++num_pos;
        }
        if (num_pos != numbers_part.size()) {
            return fail(errc::malformed_data);
        }

        const std::string name(name_part);
        const result<std::uint32_t> index = InterfaceApi::index_of(name);
        if (!index) { return fail(index.error()); }

        network_common::statistics_record record;
        record.name = name;
        record.index = *index;
        record.rx_bytes = fields[0];
        record.rx_packets = fields[1];
        record.rx_errors = fields[2];
        record.rx_dropped = fields[3];
        record.rx_multicast = fields[7];
        record.tx_bytes = fields[8];
        record.tx_packets = fields[9];
        record.tx_errors = fields[10];
        record.tx_dropped = fields[11];
        record.collisions = fields[13];
        records.push_back(std::move(record));
    }
    return records;
}

/// Reads one statistics attribute file under a sysfs interface directory.
inline result<std::uint64_t> read_sysfs_stat_file(const char* base_path,
                                                 const char* attr) {
    std::string path = base_path;
    path += "/statistics/";
    path += attr;

    result<std::string> content =
        linux_platform::read_text_file(path.c_str(), 128U);
    if (!content) { return fail(content.error()); }
    linux_platform::trim_line_end(*content);
    return parse_uint64_exact(*content);
}

/// Reads a counter that may be absent from an older sysfs implementation.
inline result<std::optional<std::uint64_t>> read_optional_sysfs_stat_file(
    const char* base_path, const char* attr) {
    const result<std::uint64_t> value =
        read_sysfs_stat_file(base_path, attr);
    if (value) { return std::optional<std::uint64_t>(*value); }
    if (value.error() == std::errc::no_such_file_or_directory ||
        value.error() == std::errc::not_a_directory) {
        return std::optional<std::uint64_t>();
    }
    return fail(value.error());
}

/// Reads statistics for an interface directly from /sys/class/net/<name>/statistics/.
template <typename InterfaceApi = native_interface_api>
inline result<network_common::statistics_record> read_sysfs_statistics(
    const std::string& name, const char* base_dir = "/sys/class/net/") {
    std::string iface_dir = base_dir;
    iface_dir += name;

    const result<std::uint32_t> index = InterfaceApi::index_of(name);
    if (!index) { return fail(index.error()); }

    network_common::statistics_record record;
    record.name = name;
    record.index = *index;

    const result<std::uint64_t> rx_bytes =
        read_sysfs_stat_file(iface_dir.c_str(), "rx_bytes");
    if (!rx_bytes) { return fail(rx_bytes.error()); }
    record.rx_bytes = *rx_bytes;

    const result<std::uint64_t> tx_bytes =
        read_sysfs_stat_file(iface_dir.c_str(), "tx_bytes");
    if (!tx_bytes) { return fail(tx_bytes.error()); }
    record.tx_bytes = *tx_bytes;

    const result<std::uint64_t> rx_packets =
        read_sysfs_stat_file(iface_dir.c_str(), "rx_packets");
    if (!rx_packets) { return fail(rx_packets.error()); }
    record.rx_packets = *rx_packets;

    const result<std::uint64_t> tx_packets =
        read_sysfs_stat_file(iface_dir.c_str(), "tx_packets");
    if (!tx_packets) { return fail(tx_packets.error()); }
    record.tx_packets = *tx_packets;

    const result<std::uint64_t> rx_errors =
        read_sysfs_stat_file(iface_dir.c_str(), "rx_errors");
    if (!rx_errors) { return fail(rx_errors.error()); }
    record.rx_errors = *rx_errors;

    const result<std::uint64_t> tx_errors =
        read_sysfs_stat_file(iface_dir.c_str(), "tx_errors");
    if (!tx_errors) { return fail(tx_errors.error()); }
    record.tx_errors = *tx_errors;

    const result<std::uint64_t> rx_dropped =
        read_sysfs_stat_file(iface_dir.c_str(), "rx_dropped");
    if (!rx_dropped) { return fail(rx_dropped.error()); }
    const result<std::uint64_t> rx_missed =
        read_sysfs_stat_file(iface_dir.c_str(), "rx_missed_errors");
    if (!rx_missed) { return fail(rx_missed.error()); }
    const result<std::uint64_t> combined_rx_dropped =
        network_common::add_statistics_counters(*rx_dropped, *rx_missed);
    if (!combined_rx_dropped) {
        return fail(combined_rx_dropped.error());
    }
    record.rx_dropped = *combined_rx_dropped;

    const result<std::uint64_t> tx_dropped =
        read_sysfs_stat_file(iface_dir.c_str(), "tx_dropped");
    if (!tx_dropped) { return fail(tx_dropped.error()); }
    record.tx_dropped = *tx_dropped;

    const result<std::optional<std::uint64_t>> multicast =
        read_optional_sysfs_stat_file(iface_dir.c_str(), "multicast");
    if (!multicast) { return fail(multicast.error()); }
    record.rx_multicast = *multicast;

    const result<std::optional<std::uint64_t>> collisions =
        read_optional_sysfs_stat_file(iface_dir.c_str(), "collisions");
    if (!collisions) { return fail(collisions.error()); }
    record.collisions = *collisions;

    return record;
}

/// Returns statistics for all network interfaces on Linux.
inline result<std::vector<network_common::statistics_record>> statistics() {
    const result<std::string> content =
        linux_platform::read_text_file("/proc/net/dev");
    if (!content) {
        return fail(content.error());
    }
    return parse_proc_net_dev(*content);
}

/// Returns statistics for a specific interface by name on Linux.
inline result<network_common::statistics_record> statistics_by_name(
    std::string_view name) {
    if (name.empty() || name.find('\0') != std::string_view::npos) {
        return fail(errc::invalid_argument);
    }
    if (!is_valid_utf8(name)) {
        return fail(errc::invalid_encoding);
    }
    const std::string name_str(name);
    result<network_common::statistics_record> sysfs_res =
        read_sysfs_statistics(name_str);
    if (sysfs_res) {
        return sysfs_res;
    }

    const bool source_absent =
        sysfs_res.error() == std::errc::no_such_file_or_directory ||
        sysfs_res.error() == std::errc::not_a_directory ||
        sysfs_res.error() == std::errc::no_such_device ||
        sysfs_res.error() == std::errc::no_such_device_or_address;
    if (!source_absent) { return fail(sysfs_res.error()); }

    // Fall back to reading /proc/net/dev
    const result<std::string> content =
        linux_platform::read_text_file("/proc/net/dev");
    if (!content) {
        return fail(content.error());
    }

    result<std::vector<network_common::statistics_record>> all =
        parse_proc_net_dev(*content);
    if (!all) { return fail(all.error()); }
    for (network_common::statistics_record& rec : *all) {
        if (rec.name == name) {
            return std::move(rec);
        }
    }
    return fail(errc::not_found);
}

/// Returns statistics for a specific interface by index on Linux.
inline result<network_common::statistics_record> statistics_by_index(
    std::uint32_t index) {
    if (index == 0U) {
        return fail(errc::invalid_argument);
    }
    char ifname[IF_NAMESIZE];
    if (::if_indextoname(index, ifname) == nullptr) {
        return fail(errc::not_found);
    }
    return statistics_by_name(ifname);
}

} // namespace network_backend
} // namespace detail
} // namespace syscape

#endif
