#ifndef SYSCAPE_DETAIL_NETWORK_ROUTES_MACOS_HPP
#define SYSCAPE_DETAIL_NETWORK_ROUTES_MACOS_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <system_error>
#include <vector>

#include <net/route.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <syscape/detail/network/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace network_backend {

inline std::size_t darwin_sockaddr_size(const ::sockaddr& address) noexcept {
    // NET_RT_DUMP2 records are produced by XNU's rt_msg2(), which uses
    // ROUNDUP32 for each sockaddr even in 64-bit processes.
    constexpr std::size_t alignment = sizeof(std::uint32_t);
    const std::size_t length =
        address.sa_len == 0U ? alignment
                             : static_cast<std::size_t>(address.sa_len);
    return (length + alignment - 1U) & ~(alignment - 1U);
}

struct darwin_route_addresses {
    const ::sockaddr* values[RTAX_MAX] {};
};

inline result<darwin_route_addresses> parse_darwin_sockaddrs(
    int mask, const unsigned char* data, std::size_t size) {
    darwin_route_addresses parsed;
    std::size_t offset = 0U;
    for (int index = 0; index < RTAX_MAX; ++index) {
        if ((mask & (1 << index)) == 0) { continue; }
        if (size - offset < 2U) {
            return fail(errc::malformed_data);
        }
        const ::sockaddr* address =
            reinterpret_cast<const ::sockaddr*>(data + offset);
        const std::size_t recorded = static_cast<std::size_t>(address->sa_len);
        const std::size_t advance = darwin_sockaddr_size(*address);
        if ((recorded != 0U && recorded < 2U) || advance > size - offset) {
            return fail(errc::malformed_data);
        }
        parsed.values[index] = address;
        offset += advance;
    }
    return parsed;
}

inline result<network_common::ip_address_record> darwin_ip_address(
    const ::sockaddr& source, int expected_family) {
    network_common::ip_address_record address;
    if (expected_family == AF_INET) {
        if (source.sa_family != AF_INET ||
            source.sa_len < sizeof(::sockaddr_in)) {
            return fail(errc::malformed_data);
        }
        const ::sockaddr_in& value =
            reinterpret_cast<const ::sockaddr_in&>(source);
        address.family = network_common::address_family::ipv4;
        std::memcpy(address.value.data(), &value.sin_addr, 4U);
    } else if (expected_family == AF_INET6) {
        if (source.sa_family != AF_INET6 ||
            source.sa_len < sizeof(::sockaddr_in6)) {
            return fail(errc::malformed_data);
        }
        const ::sockaddr_in6& value =
            reinterpret_cast<const ::sockaddr_in6&>(source);
        address.family = network_common::address_family::ipv6;
        std::memcpy(address.value.data(), &value.sin6_addr, 16U);
        address.scope_id = static_cast<std::uint32_t>(value.sin6_scope_id);
    } else {
        return fail(errc::not_supported);
    }
    return address;
}

inline result<std::uint8_t> darwin_prefix_length(const ::sockaddr* mask,
                                                  int family) {
    if (mask == nullptr) { return std::uint8_t(0U); }
    const std::size_t address_offset = family == AF_INET
                                           ? offsetof(::sockaddr_in, sin_addr)
                                           : offsetof(::sockaddr_in6, sin6_addr);
    const std::size_t address_size = family == AF_INET ? 4U : 16U;
    std::vector<unsigned char> bytes(address_size, 0U);
    const std::size_t recorded = static_cast<std::size_t>(mask->sa_len);
    if (recorded > address_offset) {
        const std::size_t available = recorded - address_offset;
        const std::size_t copied =
            available < address_size ? available : address_size;
        std::memcpy(bytes.data(),
                    reinterpret_cast<const unsigned char*>(mask) +
                        address_offset,
                    copied);
    }
    return prefix_from_netmask(
        bytes.data(), bytes.size(),
        family == AF_INET ? std::uint8_t(32U) : std::uint8_t(128U));
}

inline result<void> append_darwin_route_message(
    const ::rt_msghdr2& header, const unsigned char* payload,
    std::size_t payload_size,
    std::vector<network_common::route_record>& routes) {
    int excluded_flags = RTF_REJECT | RTF_BLACKHOLE;
#if defined(RTF_XRESOLVE)
    excluded_flags |= RTF_XRESOLVE;
#endif
#if defined(RTF_LLINFO)
    excluded_flags |= RTF_LLINFO;
#endif
#if defined(RTF_LOCAL)
    excluded_flags |= RTF_LOCAL;
#endif
#if defined(RTF_BROADCAST)
    excluded_flags |= RTF_BROADCAST;
#endif
#if defined(RTF_MULTICAST)
    excluded_flags |= RTF_MULTICAST;
#endif
#if defined(RTF_CONDEMNED)
    excluded_flags |= RTF_CONDEMNED;
#endif
#if defined(RTF_DEAD)
    excluded_flags |= RTF_DEAD;
#endif
    if ((header.rtm_flags & RTF_UP) == 0 ||
        (header.rtm_flags & excluded_flags) != 0) {
        return {};
    }
    if (header.rtm_index == 0U) { return fail(errc::malformed_data); }
    const result<darwin_route_addresses> addresses = parse_darwin_sockaddrs(
        header.rtm_addrs, payload, payload_size);
    if (!addresses) { return fail(addresses.error()); }
    const ::sockaddr* destination = addresses->values[RTAX_DST];
    if (destination == nullptr) { return fail(errc::malformed_data); }
    const int family = static_cast<int>(destination->sa_family);
    if (family != AF_INET && family != AF_INET6) { return {}; }
    result<network_common::ip_address_record> converted_destination =
        darwin_ip_address(*destination, family);
    if (!converted_destination) {
        return fail(converted_destination.error());
    }
    const result<std::uint8_t> prefix =
        darwin_prefix_length(addresses->values[RTAX_NETMASK], family);
    if (!prefix) { return fail(prefix.error()); }

    network_common::route_record record;
    record.destination = *converted_destination;
    record.prefix_length = *prefix;
    record.interface_index = static_cast<std::uint32_t>(header.rtm_index);
    const ::sockaddr* gateway = addresses->values[RTAX_GATEWAY];
    if ((header.rtm_flags & RTF_GATEWAY) != 0 && gateway != nullptr &&
        gateway->sa_family == family) {
        result<network_common::ip_address_record> converted_gateway =
            darwin_ip_address(*gateway, family);
        if (!converted_gateway) { return fail(converted_gateway.error()); }
        if (!network_common::is_unspecified(*converted_gateway)) {
            record.next_hop = *converted_gateway;
        }
    }
    const std::uint64_t metric =
        static_cast<std::uint64_t>(header.rtm_rmx.rmx_hopcount);
    if (metric > std::numeric_limits<std::uint32_t>::max()) {
        return fail(errc::value_too_large);
    }
    record.metric = static_cast<std::uint32_t>(metric);
    routes.push_back(std::move(record));
    return {};
}

inline result<std::vector<network_common::route_record>>
parse_darwin_route_dump(const unsigned char* data, std::size_t size) {
    std::vector<network_common::route_record> routes;
    std::size_t offset = 0U;
    while (offset < size) {
        if (size - offset < sizeof(::rt_msghdr2)) {
            return fail(errc::malformed_data);
        }
        ::rt_msghdr2 header {};
        std::memcpy(&header, data + offset, sizeof(header));
        const std::size_t length = static_cast<std::size_t>(header.rtm_msglen);
        if (length < sizeof(::rt_msghdr2) || length > size - offset ||
            header.rtm_version != RTM_VERSION) {
            return fail(errc::malformed_data);
        }
        const result<void> appended = append_darwin_route_message(
            header, data + offset + sizeof(::rt_msghdr2),
            length - sizeof(::rt_msghdr2), routes);
        if (!appended) { return fail(appended.error()); }
        offset += length;
    }
    return routes;
}

inline result<std::vector<network_common::route_record>> routes() {
    int management_information_base[6] = {
        CTL_NET, PF_ROUTE, 0, AF_UNSPEC, NET_RT_DUMP2, 0};
    std::size_t size = 0U;
    if (::sysctl(management_information_base, 6U, nullptr, &size, nullptr,
                 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0U) {
        return std::vector<network_common::route_record> {};
    }
    for (unsigned attempt = 0U; attempt < 3U; ++attempt) {
        std::vector<unsigned char> buffer(size);
        std::size_t actual = buffer.size();
        if (::sysctl(management_information_base, 6U, buffer.data(), &actual,
                     nullptr, 0U) == 0) {
            return parse_darwin_route_dump(buffer.data(), actual);
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

} // namespace network_backend
} // namespace detail
} // namespace syscape

#endif
