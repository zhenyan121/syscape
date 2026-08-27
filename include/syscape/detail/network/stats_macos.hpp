#ifndef SYSCAPE_DETAIL_NETWORK_STATS_MACOS_HPP
#define SYSCAPE_DETAIL_NETWORK_STATS_MACOS_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <net/if.h>
#include <net/route.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <syscape/detail/network/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace network_backend {

/// Resolves an interface index to its current operating-system name.
struct native_statistics_interface_api {
    static result<std::string> name_of(std::uint32_t index) {
        char name[IF_NAMESIZE];
        errno = 0;
        if (::if_indextoname(index, name) == nullptr) {
            const int error = errno != 0 ? errno : ENXIO;
            return fail(std::error_code(error, std::generic_category()));
        }
        return std::string(name);
    }
};

/// Parses a NET_RT_IFLIST2 dump and retains its 64-bit interface counters.
template <typename InterfaceApi = native_statistics_interface_api>
inline result<std::vector<network_common::statistics_record>>
parse_darwin_statistics_dump(const unsigned char* data, std::size_t size) {
    if (data == nullptr && size != 0U) {
        return fail(errc::malformed_data);
    }
    std::vector<network_common::statistics_record> records;
    std::size_t offset = 0U;
    while (offset < size) {
        if (size - offset < sizeof(std::uint16_t)) {
            return fail(errc::malformed_data);
        }
        std::uint16_t recorded_length = 0U;
        std::memcpy(&recorded_length, data + offset,
                    sizeof(recorded_length));
        const std::size_t length =
            static_cast<std::size_t>(recorded_length);
        if (length < 4U || length > size - offset) {
            return fail(errc::malformed_data);
        }

        const unsigned char version = data[offset + 2U];
        const unsigned char type = data[offset + 3U];
        if (version != RTM_VERSION) {
            return fail(errc::malformed_data);
        }
        if (type == RTM_IFINFO2) {
            if (length < sizeof(::if_msghdr2)) {
                return fail(errc::malformed_data);
            }
            ::if_msghdr2 header {};
            std::memcpy(&header, data + offset, sizeof(header));
            if (header.ifm_index == 0U || header.ifm_snd_drops < 0) {
                return fail(errc::malformed_data);
            }
            const result<std::string> name = InterfaceApi::name_of(
                static_cast<std::uint32_t>(header.ifm_index));
            if (!name) { return fail(name.error()); }

            network_common::statistics_record record;
            record.name = *name;
            record.index = static_cast<std::uint32_t>(header.ifm_index);
            record.rx_bytes = static_cast<std::uint64_t>(
                header.ifm_data.ifi_ibytes);
            record.tx_bytes = static_cast<std::uint64_t>(
                header.ifm_data.ifi_obytes);
            record.rx_packets = static_cast<std::uint64_t>(
                header.ifm_data.ifi_ipackets);
            record.tx_packets = static_cast<std::uint64_t>(
                header.ifm_data.ifi_opackets);
            record.rx_errors = static_cast<std::uint64_t>(
                header.ifm_data.ifi_ierrors);
            record.tx_errors = static_cast<std::uint64_t>(
                header.ifm_data.ifi_oerrors);
            record.rx_dropped = static_cast<std::uint64_t>(
                header.ifm_data.ifi_iqdrops);
            record.tx_dropped = static_cast<std::uint64_t>(
                header.ifm_snd_drops);
            record.rx_multicast = static_cast<std::uint64_t>(
                header.ifm_data.ifi_imcasts);
            record.collisions = static_cast<std::uint64_t>(
                header.ifm_data.ifi_collisions);
            records.push_back(std::move(record));
        }
        offset += length;
    }
    return records;
}

/// Returns statistics for all network interfaces on macOS.
inline result<std::vector<network_common::statistics_record>> statistics() {
    int management_information_base[6] = {
        CTL_NET, PF_ROUTE, 0, AF_UNSPEC, NET_RT_IFLIST2, 0};
    std::size_t size = 0U;
    if (::sysctl(management_information_base, 6U, nullptr, &size, nullptr,
                 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0U) {
        return std::vector<network_common::statistics_record>();
    }
    for (unsigned attempt = 0U; attempt < 3U; ++attempt) {
        std::vector<unsigned char> buffer(size);
        std::size_t actual = buffer.size();
        if (::sysctl(management_information_base, 6U, buffer.data(), &actual,
                     nullptr, 0U) == 0) {
            return parse_darwin_statistics_dump(buffer.data(), actual);
        }
        if (errno != ENOMEM) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (::sysctl(management_information_base, 6U, nullptr, &size, nullptr,
                     0U) != 0) {
            return fail(std::error_code(errno, std::generic_category()));
        }
    }
    return fail(errc::temporarily_unavailable);
}

/// Returns statistics for a specific interface by name on macOS.
inline result<network_common::statistics_record> statistics_by_name(
    std::string_view name) {
    if (name.empty() || name.find('\0') != std::string_view::npos) {
        return fail(errc::invalid_argument);
    }
    if (!is_valid_utf8(name)) {
        return fail(errc::invalid_encoding);
    }
    const result<std::vector<network_common::statistics_record>> all =
        statistics();
    if (!all) { return fail(all.error()); }
    for (const network_common::statistics_record& record : *all) {
        if (record.name == name) { return record; }
    }
    return fail(errc::not_found);
}

/// Returns statistics for a specific interface by index on macOS.
inline result<network_common::statistics_record> statistics_by_index(
    std::uint32_t index) {
    if (index == 0U) {
        return fail(errc::invalid_argument);
    }
    const result<std::vector<network_common::statistics_record>> all =
        statistics();
    if (!all) { return fail(all.error()); }
    for (const network_common::statistics_record& record : *all) {
        if (record.index == index) { return record; }
    }
    return fail(errc::not_found);
}

} // namespace network_backend
} // namespace detail
} // namespace syscape

#endif
