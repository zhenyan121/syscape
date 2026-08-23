#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iostream>
#include <limits>
#include <new>
#include <string>
#include <system_error>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <ifaddrs.h>

#include <syscape/detail/network/common.hpp>
#include <syscape/detail/network/posix.hpp>
#include <syscape/network.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

/// Injected index resolver shared by all synthetic conversions.
struct fake_index_api {
    static bool fail_resolution;
    static std::uint32_t next_index;
    static std::uint32_t mtu_bytes;

    static void reset() {
        fail_resolution = false;
        next_index = 1U;
        mtu_bytes = 1500U;
    }

    static syscape::result<std::uint32_t> index_of(const std::string&) {
        if (fail_resolution) {
            return syscape::fail(syscape::make_error_code(
                syscape::errc::not_found));
        }
        return next_index++;
    }

    static syscape::result<std::uint32_t> mtu_of(const std::string&) {
        return mtu_bytes;
    }
};

bool fake_index_api::fail_resolution = false;
std::uint32_t fake_index_api::next_index = 1U;
std::uint32_t fake_index_api::mtu_bytes = 1500U;

/// Builds synthetic getifaddrs chains whose nodes stay address-stable.
class synthetic_chain {
    struct link_storage {
        alignas(::sockaddr_dl) unsigned char bytes[256];
    };

public:
    ::ifaddrs* head() { return head_; }

    ::ifaddrs& add(const std::string& name, unsigned int flags) {
        names_.push_back(name);
        rows_.emplace_back();
        ::ifaddrs& row = rows_.back();
        row.ifa_next = nullptr;
        row.ifa_name = names_.back().data();
        row.ifa_flags = flags;
        row.ifa_addr = nullptr;
        row.ifa_netmask = nullptr;
        row.ifa_dstaddr = nullptr;
        row.ifa_data = nullptr;
        link(row);
        return row;
    }

    /// Adds an AF_LINK row carrying name_length name bytes and
    /// address_length link-layer bytes inside sdl_data.
    ::ifaddrs& add_link(const std::string& name, unsigned int flags,
                        std::size_t name_length,
                        const unsigned char* address_bytes,
                        std::size_t address_length) {
        ::ifaddrs& row = add(name, flags);
        links_.emplace_back();
        link_storage& storage = links_.back();
        static_assert(sizeof(::sockaddr_dl) <= sizeof(storage.bytes),
                      "Synthetic link storage must hold sockaddr_dl");
        ::sockaddr_dl& link = *::new (static_cast<void*>(storage.bytes))
            ::sockaddr_dl();
        const std::size_t data_offset = offsetof(::sockaddr_dl, sdl_data);
        const std::size_t total_length =
            data_offset + name_length + address_length;
        if (total_length > sizeof(storage.bytes) ||
            total_length >
                static_cast<std::size_t>(
                    (std::numeric_limits<unsigned char>::max)())) {
            std::cerr << "FAIL: synthetic AF_LINK record is too large\n";
            ++failures;
            return row;
        }
        link.sdl_family = AF_LINK;
        link.sdl_nlen = static_cast<unsigned char>(name_length);
        link.sdl_alen = static_cast<unsigned char>(address_length);
        link.sdl_len = static_cast<unsigned char>(total_length);
        if (name_length > 0U) {
            std::memcpy(storage.bytes + data_offset, name.data(), name_length);
        }
        if (address_bytes != nullptr && address_length > 0U) {
            std::memcpy(storage.bytes + data_offset + name_length,
                        address_bytes,
                        address_length);
        }
        row.ifa_addr = reinterpret_cast<::sockaddr*>(&link);
        return row;
    }

    ::ifaddrs& add_ipv6(const std::string& name, unsigned int flags,
                        std::uint32_t scope_id) {
        ::ifaddrs& row = add(name, flags);
        ipv6_.emplace_back();
        masks_ipv6_.emplace_back();
        ::sockaddr_in6& address = ipv6_.back();
        ::sockaddr_in6& mask = masks_ipv6_.back();
        std::memset(&address, 0, sizeof(address));
        std::memset(&mask, 0, sizeof(mask));
        address.sin6_family = AF_INET6;
        address.sin6_scope_id = scope_id;
        address.sin6_addr.s6_addr[0U] = 0xFEU;
        address.sin6_addr.s6_addr[1U] = 0x80U;
        address.sin6_addr.s6_addr[15U] = 1U;
        mask.sin6_family = AF_INET6;
        for (std::size_t offset = 0U; offset < 8U; ++offset) {
            mask.sin6_addr.s6_addr[offset] = 0xFFU;
        }
        row.ifa_addr = reinterpret_cast<::sockaddr*>(&address);
        row.ifa_netmask = reinterpret_cast<::sockaddr*>(&mask);
        return row;
    }

private:
    void link(::ifaddrs& row) {
        if (tail_ != nullptr) {
            tail_->ifa_next = &row;
        } else {
            head_ = &row;
        }
        tail_ = &row;
    }

    std::deque<std::string> names_;
    std::deque<::ifaddrs> rows_;
    std::deque<link_storage> links_;
    std::deque<::sockaddr_in6> ipv6_;
    std::deque<::sockaddr_in6> masks_ipv6_;
    ::ifaddrs* head_ = nullptr;
    ::ifaddrs* tail_ = nullptr;
};

void test_link_address_extraction() {
    fake_index_api::reset();
    fake_index_api::next_index = 4U;
    const unsigned char six_bytes[] = {0xA8U, 0x20U, 0x66U, 0x09U, 0xC4U,
                                       0xAAU};

    synthetic_chain chain;
    chain.add_link("en0", IFF_UP | IFF_RUNNING, 3U, six_bytes, 6U);

    const auto converted =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            chain.head());
    expect(converted && converted->size() == 1U,
           "One AF_LINK row converts into one record");
    if (!converted || converted->empty()) { return; }
    expect((*converted)[0U].index == 4U,
           "The injected resolver supplies the interface index");
    expect((*converted)[0U].mtu_bytes == 1500U,
           "The injected MTU query supplies the interface MTU");
    expect((*converted)[0U].hardware_address.size() == 6U &&
               (*converted)[0U].hardware_address[0U] == 0xA8U &&
               (*converted)[0U].hardware_address[5U] == 0xAAU,
           "The link-layer address after the interface name is copied "
           "verbatim");
}

void test_empty_link_address() {
    fake_index_api::reset();
    synthetic_chain chain;
    chain.add_link("lo0", IFF_UP | IFF_RUNNING | IFF_LOOPBACK, 3U, nullptr,
                   0U);

    const auto converted =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            chain.head());
    expect(converted && converted->size() == 1U &&
               (*converted)[0U].hardware_address.empty() &&
               (*converted)[0U].loopback,
           "A zero-length link-layer address is valid and stays empty");
}

void test_ipv6_scope_identifier() {
    fake_index_api::reset();
    synthetic_chain chain;
    chain.add_ipv6("en0", IFF_UP | IFF_RUNNING, 6U);

    const auto converted =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            chain.head());
    expect(converted && converted->size() == 1U &&
               converted->at(0U).addresses.size() == 1U &&
               converted->at(0U).addresses[0U].scope_id == 6U,
           "An IPv6 row preserves its numeric scope identifier");
}

void test_long_link_address() {
    fake_index_api::reset();
    unsigned char long_bytes[20];
    for (std::size_t offset = 0U; offset < 20U; ++offset) {
        long_bytes[offset] = static_cast<unsigned char>(offset);
    }

    synthetic_chain chain;
    chain.add_link("ib0", IFF_UP | IFF_RUNNING, 3U, long_bytes, 20U);

    const auto converted =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            chain.head());
    expect(converted &&
               converted->at(0U).hardware_address.size() == 20U &&
               converted->at(0U).hardware_address[19U] == 19U,
           "A link-layer address longer than six bytes is preserved "
           "without truncation");
}

void test_inconsistent_link_record_is_malformed() {
    fake_index_api::reset();
    synthetic_chain chain;
    // The recorded sdl_len cannot cover the declared address length.
    ::ifaddrs& row = chain.add_link("bad0", IFF_UP, 3U, nullptr, 0U);
    ::sockaddr_dl* link = reinterpret_cast<::sockaddr_dl*>(row.ifa_addr);
    link->sdl_alen = 6U;
    link->sdl_len = static_cast<unsigned char>(
        offsetof(::sockaddr_dl, sdl_data) + 3U);

    const auto converted =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            chain.head());
    expect(!converted && converted.error() ==
                              syscape::make_error_code(
                                  syscape::errc::malformed_data),
           "A link record whose storage cannot hold its declared address "
           "is malformed platform data");
}

void test_index_resolution_failure() {
    fake_index_api::reset();
    fake_index_api::fail_resolution = true;
    synthetic_chain chain;
    chain.add("en0", IFF_UP | IFF_RUNNING);
    const auto converted =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            chain.head());
    expect(!converted && converted.error() ==
                              syscape::make_error_code(
                                  syscape::errc::not_found),
           "A failed interface-index resolution fails the snapshot");
}

void test_live_enumeration() {
    const auto interfaces = syscape::network::interfaces();
    if (!interfaces) {
        const std::error_code error = interfaces.error();
        expect(error == std::errc::permission_denied ||
                   error == std::errc::operation_not_permitted ||
                   error == std::errc::operation_not_supported,
               "Live enumeration fails only when the environment denies "
               "or does not expose the required capability");
        return;
    }
    expect(!interfaces->empty(),
           "A running hosted system exposes at least one interface");

    bool has_loopback = false;
    for (const syscape::network::interface_entry& entry : *interfaces) {
        expect(entry.index != 0U,
               "Every live interface has a nonzero index");
        expect(entry.mtu_bytes != 0U,
               "Every live interface has a nonzero MTU");
        expect(!entry.name.empty(), "Every live interface has a name");
        has_loopback = has_loopback || entry.loopback;
    }
    expect(has_loopback, "This host exposes a loopback interface");

    // Cross-check against an independent getifaddrs walk over the same
    // live table.
    ::ifaddrs* list = nullptr;
    if (::getifaddrs(&list) != 0) { return; }
    std::size_t independent_links = 0U;
    for (const ::ifaddrs* cursor = list; cursor != nullptr;
         cursor = cursor->ifa_next) {
        if (cursor->ifa_addr != nullptr &&
            cursor->ifa_addr->sa_family == AF_LINK) {
            ++independent_links;
        }
    }
    ::freeifaddrs(list);
    expect(independent_links > 0U,
           "The live table exposes link-layer rows for the cross-check");
}

void append_darwin_address(std::vector<unsigned char>& message,
                           const void* address, std::size_t recorded_size) {
    const std::size_t alignment = sizeof(std::uint32_t);
    const std::size_t storage_size =
        (recorded_size + alignment - 1U) & ~(alignment - 1U);
    const std::size_t offset = message.size();
    message.resize(offset + storage_size, 0U);
    std::memcpy(message.data() + offset, address, recorded_size);
}

void test_darwin_route_dump_conversion() {
    ::rt_msghdr2 header {};
    header.rtm_version = RTM_VERSION;
    header.rtm_type = RTM_GET2;
    header.rtm_index = 6U;
    header.rtm_flags = RTF_UP | RTF_GATEWAY;
    header.rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
    header.rtm_rmx.rmx_hopcount = 3U;
    std::vector<unsigned char> message(sizeof(header));

    ::sockaddr_in destination {};
    destination.sin_len = sizeof(destination);
    destination.sin_family = AF_INET;
    append_darwin_address(message, &destination, sizeof(destination));
    ::sockaddr_in gateway {};
    gateway.sin_len = sizeof(gateway);
    gateway.sin_family = AF_INET;
    gateway.sin_addr.s_addr = htonl(0xC0A80101U);
    append_darwin_address(message, &gateway, sizeof(gateway));
    ::sockaddr_in mask {};
    mask.sin_len = sizeof(mask);
    append_darwin_address(message, &mask, sizeof(mask));

    header.rtm_msglen = static_cast<unsigned short>(message.size());
    std::memcpy(message.data(), &header, sizeof(header));
    const auto converted =
        syscape::detail::network_backend::parse_darwin_route_dump(
            message.data(), message.size());
    expect(converted && converted->size() == 1U &&
               converted->at(0U).prefix_length == 0U &&
               converted->at(0U).next_hop &&
               converted->at(0U).next_hop->value[0U] == 192U &&
               converted->at(0U).interface_index == 6U &&
               converted->at(0U).metric &&
               *converted->at(0U).metric == 3U,
           "A Darwin default route keeps its gateway, interface, and metric");

    message.pop_back();
    const auto truncated =
        syscape::detail::network_backend::parse_darwin_route_dump(
            message.data(), message.size());
    expect(!truncated && truncated.error() == syscape::errc::malformed_data,
           "A truncated Darwin route message is malformed");
}

void test_darwin_ipv6_sockaddr_alignment() {
    ::rt_msghdr2 header {};
    header.rtm_version = RTM_VERSION;
    header.rtm_type = RTM_GET2;
    header.rtm_index = 9U;
    header.rtm_flags = RTF_UP | RTF_GATEWAY;
    header.rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
    std::vector<unsigned char> message(sizeof(header));

    ::sockaddr_in6 destination {};
    destination.sin6_len = sizeof(destination);
    destination.sin6_family = AF_INET6;
    append_darwin_address(message, &destination, sizeof(destination));
    ::sockaddr_in6 gateway {};
    gateway.sin6_len = sizeof(gateway);
    gateway.sin6_family = AF_INET6;
    gateway.sin6_addr.s6_addr[0U] = 0xFEU;
    gateway.sin6_addr.s6_addr[1U] = 0x80U;
    gateway.sin6_addr.s6_addr[15U] = 1U;
    gateway.sin6_scope_id = 9U;
    append_darwin_address(message, &gateway, sizeof(gateway));
    ::sockaddr mask {};
    append_darwin_address(message, &mask, sizeof(std::uint32_t));

    header.rtm_msglen = static_cast<unsigned short>(message.size());
    std::memcpy(message.data(), &header, sizeof(header));
    const auto converted =
        syscape::detail::network_backend::parse_darwin_route_dump(
            message.data(), message.size());
    expect(converted && converted->size() == 1U &&
               converted->at(0U).next_hop &&
               converted->at(0U).next_hop->value[0U] == 0xFEU &&
               converted->at(0U).next_hop->scope_id == 9U,
           "Darwin NET_RT_DUMP2 uses four-byte sockaddr alignment");
}

void test_darwin_unusable_routes_are_filtered() {
    ::rt_msghdr2 header {};
    header.rtm_version = RTM_VERSION;
    header.rtm_type = RTM_GET2;
    header.rtm_flags = RTF_UP;
#if defined(RTF_BROADCAST)
    header.rtm_flags |= RTF_BROADCAST;
#elif defined(RTF_MULTICAST)
    header.rtm_flags |= RTF_MULTICAST;
#else
    header.rtm_flags |= RTF_REJECT;
#endif
    std::vector<syscape::detail::network_common::route_record> routes;
    const auto converted =
        syscape::detail::network_backend::append_darwin_route_message(
            header, nullptr, 0U, routes);
    expect(converted && routes.empty(),
           "Darwin broadcast and other unusable routes are omitted");

    header.rtm_flags = RTF_GATEWAY;
    const auto down =
        syscape::detail::network_backend::append_darwin_route_message(
            header, nullptr, 0U, routes);
    expect(down && routes.empty(), "Darwin routes that are not up are omitted");
}

} // namespace

int main() {
    test_link_address_extraction();
    test_empty_link_address();
    test_ipv6_scope_identifier();
    test_long_link_address();
    test_inconsistent_link_record_is_malformed();
    test_index_resolution_failure();
    test_live_enumeration();
    test_darwin_route_dump_conversion();
    test_darwin_ipv6_sockaddr_alignment();
    test_darwin_unusable_routes_are_filtered();
    return failures == 0 ? 0 : 1;
}
