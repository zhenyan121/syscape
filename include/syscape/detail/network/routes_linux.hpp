#ifndef SYSCAPE_DETAIL_NETWORK_ROUTES_LINUX_HPP
#define SYSCAPE_DETAIL_NETWORK_ROUTES_LINUX_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <system_error>
#include <vector>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <unistd.h>

#include <syscape/detail/network/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace network_backend {

class netlink_socket_guard {
public:
    explicit netlink_socket_guard(int value) noexcept : value_(value) {}
    netlink_socket_guard(const netlink_socket_guard&) = delete;
    netlink_socket_guard& operator=(const netlink_socket_guard&) = delete;
    ~netlink_socket_guard() {
        if (value_ >= 0) { ::close(value_); }
    }
    int get() const noexcept { return value_; }

private:
    int value_;
};

inline std::size_t netlink_align(std::size_t value) noexcept {
    return (value + 3U) & ~std::size_t(3U);
}

inline result<network_common::ip_address_record> netlink_address(
    int family, const unsigned char* data, std::size_t size) {
    network_common::ip_address_record address;
    std::size_t expected = 0U;
    if (family == AF_INET) {
        address.family = network_common::address_family::ipv4;
        expected = 4U;
    } else if (family == AF_INET6) {
        address.family = network_common::address_family::ipv6;
        expected = 16U;
    } else {
        return fail(errc::not_supported);
    }
    if (size != expected) { return fail(errc::malformed_data); }
    std::memcpy(address.value.data(), data, expected);
    return address;
}

struct linux_route_attributes {
    std::optional<network_common::ip_address_record> destination;
    std::optional<network_common::ip_address_record> gateway;
    std::optional<std::uint32_t> interface_index;
    std::optional<std::uint32_t> metric;
    bool has_nexthop_id = false;
    const unsigned char* multipath = nullptr;
    std::size_t multipath_size = 0U;
};

// RTA_NH_ID has the stable Linux UAPI value 30. Spell out the protocol value
// so Syscape can still compile against older libc kernel-header snapshots.
constexpr std::uint16_t linux_rta_nh_id = 30U;

inline result<linux_route_attributes> parse_route_attributes(
    int family, const unsigned char* data, std::size_t size) {
    linux_route_attributes parsed;
    std::size_t offset = 0U;
    while (offset < size) {
        if (size - offset < sizeof(::rtattr)) {
            return fail(errc::malformed_data);
        }
        ::rtattr attribute {};
        std::memcpy(&attribute, data + offset, sizeof(attribute));
        const std::size_t length = static_cast<std::size_t>(attribute.rta_len);
        if (length < sizeof(::rtattr) || length > size - offset) {
            return fail(errc::malformed_data);
        }
        const unsigned char* payload = data + offset + sizeof(::rtattr);
        const std::size_t payload_size = length - sizeof(::rtattr);
        if (attribute.rta_type == RTA_DST) {
            result<network_common::ip_address_record> address =
                netlink_address(family, payload, payload_size);
            if (!address) { return fail(address.error()); }
            parsed.destination = *address;
        } else if (attribute.rta_type == RTA_GATEWAY) {
            result<network_common::ip_address_record> address =
                netlink_address(family, payload, payload_size);
            if (!address) { return fail(address.error()); }
            parsed.gateway = *address;
        } else if (attribute.rta_type == RTA_OIF) {
            if (payload_size != sizeof(std::uint32_t)) {
                return fail(errc::malformed_data);
            }
            std::uint32_t value = 0U;
            std::memcpy(&value, payload, sizeof(value));
            parsed.interface_index = value;
        } else if (attribute.rta_type == RTA_PRIORITY) {
            if (payload_size != sizeof(std::uint32_t)) {
                return fail(errc::malformed_data);
            }
            std::uint32_t value = 0U;
            std::memcpy(&value, payload, sizeof(value));
            parsed.metric = value;
        } else if (attribute.rta_type == RTA_MULTIPATH) {
            parsed.multipath = payload;
            parsed.multipath_size = payload_size;
        } else if (attribute.rta_type == linux_rta_nh_id) {
            if (payload_size != sizeof(std::uint32_t)) {
                return fail(errc::malformed_data);
            }
            std::uint32_t value = 0U;
            std::memcpy(&value, payload, sizeof(value));
            if (value == 0U) { return fail(errc::malformed_data); }
            parsed.has_nexthop_id = true;
        } else if (attribute.rta_type == RTA_VIA) {
            return fail(errc::not_supported);
        }
        const std::size_t advance = netlink_align(length);
        if (advance < length || advance > size - offset) {
            return fail(errc::malformed_data);
        }
        offset += advance;
    }
    return parsed;
}

inline bool ipv6_link_local(
    const network_common::ip_address_record& address) noexcept {
    return address.family == network_common::address_family::ipv6 &&
           address.value[0U] == 0xFEU &&
           (address.value[1U] & 0xC0U) == 0x80U;
}

inline network_common::route_record make_linux_route(
    int family, std::uint8_t prefix_length,
    const linux_route_attributes& attributes, std::uint32_t interface_index,
    std::optional<network_common::ip_address_record> gateway) {
    network_common::route_record record;
    record.destination.family = family == AF_INET
                                    ? network_common::address_family::ipv4
                                    : network_common::address_family::ipv6;
    if (attributes.destination) {
        record.destination = *attributes.destination;
    }
    record.prefix_length = prefix_length;
    if (gateway && ipv6_link_local(*gateway)) {
        gateway->scope_id = interface_index;
    }
    record.next_hop = gateway;
    record.interface_index = interface_index;
    record.metric = attributes.metric;
    return record;
}

inline result<void> append_linux_multipath_routes(
    int family, std::uint8_t prefix_length,
    const linux_route_attributes& attributes,
    std::vector<network_common::route_record>& routes) {
    if (attributes.multipath_size == 0U) {
        return fail(errc::malformed_data);
    }
    std::size_t offset = 0U;
    while (offset < attributes.multipath_size) {
        if (attributes.multipath_size - offset < sizeof(::rtnexthop)) {
            return fail(errc::malformed_data);
        }
        ::rtnexthop hop {};
        std::memcpy(&hop, attributes.multipath + offset, sizeof(hop));
        const std::size_t length = static_cast<std::size_t>(hop.rtnh_len);
        if (length < sizeof(::rtnexthop) ||
            length > attributes.multipath_size - offset ||
            hop.rtnh_ifindex <= 0) {
            return fail(errc::malformed_data);
        }
        const result<linux_route_attributes> nested = parse_route_attributes(
            family, attributes.multipath + offset + sizeof(::rtnexthop),
            length - sizeof(::rtnexthop));
        if (!nested) { return fail(nested.error()); }
        routes.push_back(make_linux_route(
            family, prefix_length, attributes,
            static_cast<std::uint32_t>(hop.rtnh_ifindex), nested->gateway));
        const std::size_t advance = netlink_align(length);
        if (advance < length || advance > attributes.multipath_size - offset) {
            return fail(errc::malformed_data);
        }
        offset += advance;
    }
    return {};
}

inline result<void> append_linux_route_message(
    const unsigned char* data, std::size_t size,
    std::vector<network_common::route_record>& routes) {
    if (size < sizeof(::rtmsg)) { return fail(errc::malformed_data); }
    ::rtmsg message {};
    std::memcpy(&message, data, sizeof(message));
    if (message.rtm_family != AF_INET && message.rtm_family != AF_INET6) {
        return {};
    }
    if (message.rtm_type != RTN_UNICAST) { return {}; }
    if (message.rtm_dst_len >
        network_common::maximum_prefix_length(
            message.rtm_family == AF_INET
                ? network_common::address_family::ipv4
                : network_common::address_family::ipv6)) {
        return fail(errc::malformed_data);
    }
    const std::size_t attribute_offset = netlink_align(sizeof(::rtmsg));
    if (attribute_offset > size) { return fail(errc::malformed_data); }
    const result<linux_route_attributes> attributes = parse_route_attributes(
        message.rtm_family, data + attribute_offset, size - attribute_offset);
    if (!attributes) { return fail(attributes.error()); }
    if (message.rtm_dst_len != 0U && !attributes->destination) {
        return fail(errc::malformed_data);
    }
    if (attributes->multipath != nullptr) {
        return append_linux_multipath_routes(
            message.rtm_family, message.rtm_dst_len, *attributes, routes);
    }
    if (!attributes->interface_index || *attributes->interface_index == 0U) {
        if (attributes->has_nexthop_id) {
            // Resolving a standalone kernel nexthop object requires a
            // separate RTM_GETNEXTHOP dump. Do not misclassify a valid
            // compact route record as malformed platform data.
            return fail(errc::not_supported);
        }
        return fail(errc::malformed_data);
    }
    routes.push_back(make_linux_route(
        message.rtm_family, message.rtm_dst_len, *attributes,
        *attributes->interface_index, attributes->gateway));
    return {};
}

struct linux_route_dump_state {
    std::vector<network_common::route_record> routes;
    bool complete = false;
};

inline result<void> parse_linux_route_datagram(
    const unsigned char* data, std::size_t size, std::uint32_t sequence,
    linux_route_dump_state& state) {
    std::size_t offset = 0U;
    while (offset < size) {
        if (size - offset < sizeof(::nlmsghdr)) {
            return fail(errc::malformed_data);
        }
        ::nlmsghdr header {};
        std::memcpy(&header, data + offset, sizeof(header));
        const std::size_t length = static_cast<std::size_t>(header.nlmsg_len);
        if (length < sizeof(::nlmsghdr) || length > size - offset ||
            header.nlmsg_seq != sequence) {
            return fail(errc::malformed_data);
        }
        if ((header.nlmsg_flags & NLM_F_DUMP_INTR) != 0U) {
            return fail(errc::temporarily_unavailable);
        }
        const unsigned char* payload = data + offset + sizeof(::nlmsghdr);
        const std::size_t payload_size = length - sizeof(::nlmsghdr);
        if (header.nlmsg_type == NLMSG_DONE) {
            state.complete = true;
        } else if (header.nlmsg_type == NLMSG_ERROR) {
            if (payload_size < sizeof(::nlmsgerr)) {
                return fail(errc::malformed_data);
            }
            ::nlmsgerr error {};
            std::memcpy(&error, payload, sizeof(error));
            if (error.error != 0) {
                const int native = error.error < 0 ? -error.error : error.error;
                return fail(std::error_code(native, std::generic_category()));
            }
        } else if (header.nlmsg_type == RTM_NEWROUTE) {
            const result<void> appended = append_linux_route_message(
                payload, payload_size, state.routes);
            if (!appended) { return fail(appended.error()); }
        } else if (header.nlmsg_type == NLMSG_OVERRUN) {
            return fail(errc::temporarily_unavailable);
        }
        const std::size_t advance = netlink_align(length);
        if (advance < length || advance > size - offset) {
            return fail(errc::malformed_data);
        }
        offset += advance;
    }
    return {};
}

inline result<std::vector<network_common::route_record>> routes() {
    const int descriptor = ::socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (descriptor < 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    const netlink_socket_guard guard(descriptor);
    ::sockaddr_nl local {};
    local.nl_family = AF_NETLINK;
    if (::bind(descriptor, reinterpret_cast<const ::sockaddr*>(&local),
               sizeof(local)) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }

    struct request_type {
        ::nlmsghdr header;
        ::rtmsg message;
    } request {};
    constexpr std::uint32_t sequence = 1U;
    request.header.nlmsg_len = static_cast<std::uint32_t>(sizeof(request));
    request.header.nlmsg_type = RTM_GETROUTE;
    request.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    request.header.nlmsg_seq = sequence;
    request.message.rtm_family = AF_UNSPEC;

    ssize_t sent;
    do {
        sent = ::send(descriptor, &request, sizeof(request), 0);
    } while (sent < 0 && errno == EINTR);
    if (sent < 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (static_cast<std::size_t>(sent) != sizeof(request)) {
        return fail(errc::io_error);
    }

    linux_route_dump_state state;
    std::vector<unsigned char> buffer(64U * 1024U);
    while (!state.complete) {
        ::sockaddr_nl sender {};
        ::iovec vector {buffer.data(), buffer.size()};
        ::msghdr message {};
        message.msg_name = &sender;
        message.msg_namelen = sizeof(sender);
        message.msg_iov = &vector;
        message.msg_iovlen = 1U;
        ssize_t received;
        do {
            received = ::recvmsg(descriptor, &message, 0);
        } while (received < 0 && errno == EINTR);
        if (received < 0) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (received == 0) { return fail(errc::temporarily_unavailable); }
        if ((message.msg_flags & MSG_TRUNC) != 0 || sender.nl_pid != 0U) {
            return fail(errc::malformed_data);
        }
        const result<void> parsed = parse_linux_route_datagram(
            buffer.data(), static_cast<std::size_t>(received), sequence, state);
        if (!parsed) { return fail(parsed.error()); }
    }
    return state.routes;
}

} // namespace network_backend
} // namespace detail
} // namespace syscape

#endif
