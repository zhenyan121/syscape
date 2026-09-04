#ifndef SYSCAPE_DETAIL_NETWORK_POSIX_HPP
#define SYSCAPE_DETAIL_NETWORK_POSIX_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <system_error>
#include <vector>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#if defined(__sun) || defined(__sun__)
#include <sys/sockio.h>
#endif
#if defined(__HAIKU__)
#include <sys/sockio.h>
#endif
#include <net/if.h>
#include <netinet/in.h>
#if defined(__HAIKU__) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#define SYSCAPE_DETAIL_NETWORK_UNDEFINE_DEFAULT_SOURCE
#endif
#include <ifaddrs.h>
#if defined(SYSCAPE_DETAIL_NETWORK_UNDEFINE_DEFAULT_SOURCE)
#undef SYSCAPE_DETAIL_NETWORK_UNDEFINE_DEFAULT_SOURCE
#undef _DEFAULT_SOURCE
#endif
#include <unistd.h>
#if defined(AF_PACKET)
#include <netpacket/packet.h>
#elif defined(AF_LINK)
#include <net/if_dl.h>
#endif

#include <syscape/detail/network/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace network_backend {

/// Owns a getifaddrs list and releases it with freeifaddrs.
#if !defined(__ANDROID__) || __ANDROID_API__ >= 24
class ifaddrs_guard {
public:
    explicit ifaddrs_guard(::ifaddrs* value) noexcept : value_(value) {}
    ifaddrs_guard(const ifaddrs_guard&) = delete;
    ifaddrs_guard& operator=(const ifaddrs_guard&) = delete;
    ~ifaddrs_guard() {
        if (value_ != nullptr) { ::freeifaddrs(value_); }
    }

    ::ifaddrs* get() const noexcept { return value_; }

private:
    ::ifaddrs* value_;
};
#endif

/// Owns the socket used for read-only interface ioctls.
class socket_guard {
public:
    explicit socket_guard(int value) noexcept : value_(value) {}
    socket_guard(const socket_guard&) = delete;
    socket_guard& operator=(const socket_guard&) = delete;
    ~socket_guard() {
        if (value_ >= 0) { ::close(value_); }
    }

    int get() const noexcept { return value_; }

private:
    int value_;
};

/// Native MTU ioctl used by the retryable conversion boundary.
struct native_mtu_ioctl_api {
    static int get(int descriptor, ::ifreq* request) noexcept {
        return ::ioctl(descriptor, SIOCGIFMTU, request);
    }
};

/// Executes an MTU ioctl, retrying interruption and validating its signed
/// native result before converting it to the portable unsigned byte count.
template <typename IoctlApi>
inline result<std::uint32_t> read_mtu(int descriptor, ::ifreq& request) {
    int outcome;
    do {
        outcome = IoctlApi::get(descriptor, &request);
    } while (outcome != 0 && errno == EINTR);
    if (outcome != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (request.ifr_mtu < 0) { return fail(errc::malformed_data); }
    return static_cast<std::uint32_t>(request.ifr_mtu);
}

/// Platform calls used to convert a getifaddrs list.
///
/// The indirection exists so tests can drive the conversion with injected
/// index resolution; production callers always use the native implementation.
struct native_interface_api {
    /// Resolves an interface name to its nonzero interface index through
    /// POSIX if_nametoindex.
    static result<std::uint32_t> index_of(const std::string& name) {
        errno = 0;
        const unsigned resolved = ::if_nametoindex(name.c_str());
        if (resolved == 0U) {
            const int error = errno != 0 ? errno : ENXIO;
            return fail(std::error_code(error, std::generic_category()));
        }
        return static_cast<std::uint32_t>(resolved);
    }

    /// Returns the interface MTU in bytes through the documented
    /// SIOCGIFMTU interface.
    static result<std::uint32_t> mtu_of(const std::string& name) {
        if (name.size() >= static_cast<std::size_t>(IFNAMSIZ)) {
            return fail(errc::malformed_data);
        }
        const int descriptor = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (descriptor < 0) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        const socket_guard guard(descriptor);
        ::ifreq request {};
        std::memcpy(request.ifr_name, name.data(), name.size());
        return read_mtu<native_mtu_ioctl_api>(guard.get(), request);
    }
};

/// Returns the portable state for an interface flags word.
///
/// An interface that is administratively up but not running (for example a
/// disconnected cable) is neither plainly up nor plainly down, so it is
/// reported as unknown rather than forced into either value.
template <typename Flags>
inline network_common::interface_state classify_state(Flags flags) noexcept {
    if ((flags & static_cast<Flags>(IFF_UP)) == static_cast<Flags>(0)) {
        return network_common::interface_state::down;
    }
#if defined(__HAIKU__)
    if ((flags & static_cast<Flags>(IFF_LINK)) != static_cast<Flags>(0)) {
#else
    if ((flags & static_cast<Flags>(IFF_RUNNING)) != static_cast<Flags>(0)) {
#endif
        return network_common::interface_state::up;
    }
    return network_common::interface_state::unknown;
}

/// Converts a contiguous netmask into its prefix length in bits.
///
/// A documented netmask is a run of one bits followed only by zero bits.
/// Any one bit after the first zero bit is malformed platform data because
/// no documented producer emits it and no prefix length can express it.
inline result<std::uint8_t> prefix_from_netmask(
    const unsigned char* bytes, std::size_t size,
    std::uint8_t maximum) noexcept {
    std::uint8_t prefix = 0U;
    bool partial_seen = false;
    for (std::size_t offset = 0U; offset < size; ++offset) {
        const unsigned char byte = bytes[offset];
        if (partial_seen) {
            if (byte != 0U) { return fail(errc::malformed_data); }
            continue;
        }
        if (byte == 0xFFU) {
            prefix = static_cast<std::uint8_t>(prefix + 8U);
            continue;
        }
        unsigned char mask = 0x80U;
        while (mask != 0U && (byte & mask) != 0U) {
            prefix = static_cast<std::uint8_t>(prefix + 1U);
            mask = static_cast<unsigned char>(mask >> 1U);
        }
        // mask >= 0x01 here because a fully-set byte was handled above.
        if ((byte & static_cast<unsigned char>(mask - 1U)) != 0U) {
            return fail(errc::malformed_data);
        }
        partial_seen = true;
    }
    if (prefix > maximum) { return fail(errc::malformed_data); }
    return prefix;
}

/// Copies the address portion of a possibly shortened native netmask.
///
/// BSD routing interfaces may omit trailing zero bytes from a sockaddr
/// netmask. The destination is therefore cleared first and only bytes covered
/// by the native record are copied.
inline void copy_recorded_netmask_bytes(const unsigned char* source,
                                        std::size_t recorded_size,
                                        std::size_t address_offset,
                                        unsigned char* destination,
                                        std::size_t destination_size) noexcept {
    std::memset(destination, 0, destination_size);
    if (recorded_size <= address_offset) {
        return;
    }
    const std::size_t available = recorded_size - address_offset;
    const std::size_t copy_size =
        available < destination_size ? available : destination_size;
    std::memcpy(destination, source + address_offset, copy_size);
}

/// Extracts the address bytes from a POSIX netmask sockaddr.
inline void copy_netmask_bytes(const ::sockaddr& netmask,
                               std::size_t address_offset,
                               unsigned char* destination,
                               std::size_t destination_size) noexcept {
    std::size_t recorded_size = address_offset + destination_size;
#if defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__) ||     \
    defined(__DragonFly__) || defined(__APPLE__)
    recorded_size = static_cast<std::size_t>(netmask.sa_len);
#endif
    copy_recorded_netmask_bytes(
        reinterpret_cast<const unsigned char*>(&netmask), recorded_size,
        address_offset, destination, destination_size);
}

/// Returns the record for an interface name, creating it on first sight so
/// that the platform enumeration order is preserved.
inline network_common::interface_record* find_interface_by_name(
    std::vector<network_common::interface_record>& interfaces,
    const std::string& name) {
    for (network_common::interface_record& entry : interfaces) {
        if (entry.name == name) { return &entry; }
    }
    return nullptr;
}

/// Converts one IPv4 row into a unicast record.
inline result<network_common::unicast_record> make_ipv4_record(
    const unsigned char* address, const unsigned char* netmask) noexcept {
    network_common::unicast_record record;
    record.family = network_common::address_family::ipv4;
    for (std::size_t offset = 0U; offset < 4U; ++offset) {
        record.value[offset] = address[offset];
    }
    if (netmask == nullptr) {
        return fail(errc::malformed_data);
    }
    const result<std::uint8_t> prefix =
        prefix_from_netmask(netmask, 4U, network_common::maximum_prefix_length(
                                            record.family));
    if (!prefix) { return fail(prefix.error()); }
    record.prefix_length = *prefix;
    return record;
}

/// Converts one IPv6 row into a unicast record.
inline result<network_common::unicast_record> make_ipv6_record(
    const unsigned char* address, const unsigned char* netmask,
    std::uint32_t scope_id) noexcept {
    network_common::unicast_record record;
    record.family = network_common::address_family::ipv6;
    for (std::size_t offset = 0U; offset < 16U; ++offset) {
        record.value[offset] = address[offset];
    }
    if (scope_id != 0U && record.value[0] == 0xFE &&
        (record.value[1] & 0xC0) == 0x80) {
        record.value[2] = 0;
        record.value[3] = 0;
    }
    if (netmask == nullptr) {
        return fail(errc::malformed_data);
    }
    const result<std::uint8_t> prefix =
        prefix_from_netmask(netmask, 16U, network_common::maximum_prefix_length(
                                            record.family));
    if (!prefix) { return fail(prefix.error()); }
    record.prefix_length = *prefix;
    record.scope_id = scope_id;
    return record;
}

/// Converts one address row of a getifaddrs list into the shared record
/// shape. Rows of families this slice does not represent are skipped.
template <typename InterfaceApi>
inline result<void> convert_ifaddrs_row(
    const ::ifaddrs& row,
    std::vector<network_common::interface_record>& interfaces) {
    if (row.ifa_name == nullptr) { return fail(errc::malformed_data); }
    const std::string name(row.ifa_name);

    network_common::interface_record* entry =
        find_interface_by_name(interfaces, name);
    if (entry == nullptr) {
        const result<std::uint32_t> index = InterfaceApi::index_of(name);
        if (!index) { return fail(index.error()); }
        network_common::interface_record created;
        created.name = name;
        created.index = *index;
        const result<std::uint32_t> mtu = InterfaceApi::mtu_of(name);
        if (mtu) {
            created.mtu_bytes = *mtu;
        } else {
#if defined(AF_LINK) && !defined(__HAIKU__)
            if (row.ifa_data != nullptr) {
                const struct ::if_data* data =
                    reinterpret_cast<const struct ::if_data*>(row.ifa_data);
                if (data->ifi_mtu > 0) {
                    created.mtu_bytes =
                        static_cast<std::uint32_t>(data->ifi_mtu);
                } else {
                    return fail(mtu.error());
                }
            } else {
                return fail(mtu.error());
            }
#else
            return fail(mtu.error());
#endif
        }
        interfaces.push_back(std::move(created));
        entry = &interfaces.back();
    }

    using flags_type = decltype(row.ifa_flags);
    const flags_type flags = row.ifa_flags;
    entry->state = classify_state(flags);
    entry->loopback = (flags & static_cast<flags_type>(IFF_LOOPBACK)) !=
                      static_cast<flags_type>(0);

    if (row.ifa_addr == nullptr) {
        // A row without a socket address contributes only flags.
        return {};
    }
    const unsigned int family =
        static_cast<unsigned int>(row.ifa_addr->sa_family);

    if (family == static_cast<unsigned int>(AF_INET)) {
        if (row.ifa_netmask != nullptr &&
            static_cast<unsigned int>(row.ifa_netmask->sa_family) !=
                static_cast<unsigned int>(AF_INET) &&
            static_cast<unsigned int>(row.ifa_netmask->sa_family) !=
                static_cast<unsigned int>(AF_UNSPEC) &&
            row.ifa_netmask->sa_family != 0) {
            return fail(errc::malformed_data);
        }
        const ::sockaddr_in* address =
            reinterpret_cast<const ::sockaddr_in*>(row.ifa_addr);
        unsigned char netmask_bytes[4] {};
        if (row.ifa_netmask != nullptr) {
            copy_netmask_bytes(*row.ifa_netmask,
                               offsetof(::sockaddr_in, sin_addr), netmask_bytes,
                               sizeof(netmask_bytes));
        }
        result<network_common::unicast_record> record = make_ipv4_record(
            reinterpret_cast<const unsigned char*>(&address->sin_addr),
            row.ifa_netmask != nullptr ? netmask_bytes : nullptr);
        if (!record) { return fail(record.error()); }
        entry->addresses.push_back(std::move(*record));
        return {};
    }

    if (family == static_cast<unsigned int>(AF_INET6)) {
        if (row.ifa_netmask != nullptr &&
            static_cast<unsigned int>(row.ifa_netmask->sa_family) !=
                static_cast<unsigned int>(AF_INET6) &&
            static_cast<unsigned int>(row.ifa_netmask->sa_family) !=
                static_cast<unsigned int>(AF_UNSPEC) &&
            row.ifa_netmask->sa_family != 0) {
            return fail(errc::malformed_data);
        }
        const ::sockaddr_in6* address =
            reinterpret_cast<const ::sockaddr_in6*>(row.ifa_addr);
        unsigned char netmask_bytes[16] {};
        if (row.ifa_netmask != nullptr) {
            copy_netmask_bytes(*row.ifa_netmask,
                               offsetof(::sockaddr_in6, sin6_addr),
                               netmask_bytes, sizeof(netmask_bytes));
        }
        std::uint32_t scope_id = 0U;
#if defined(SIN6_LEN) || defined(__OpenBSD__) || defined(__FreeBSD__) ||       \
    defined(__NetBSD__) || defined(__DragonFly__) || defined(__APPLE__)
        if (address->sin6_scope_id != 0U) {
            scope_id = static_cast<std::uint32_t>(address->sin6_scope_id);
        } else if (address->sin6_addr.s6_addr[0] == 0xFE &&
                   (address->sin6_addr.s6_addr[1] & 0xC0) == 0x80) {
            scope_id =
                (static_cast<std::uint32_t>(address->sin6_addr.s6_addr[2])
                 << 8) |
                address->sin6_addr.s6_addr[3];
        }
#else
        scope_id = static_cast<std::uint32_t>(address->sin6_scope_id);
#endif
        result<network_common::unicast_record> record = make_ipv6_record(
            reinterpret_cast<const unsigned char*>(&address->sin6_addr),
            row.ifa_netmask != nullptr ? netmask_bytes : nullptr, scope_id);
        if (!record) { return fail(record.error()); }
        entry->addresses.push_back(std::move(*record));
        return {};
    }

#if defined(AF_PACKET)
    // Linux exposes the link-layer address through an AF_PACKET row. The
    // sockaddr_ll storage holds at most sizeof(sll_addr) bytes, so a longer
    // recorded hardware length cannot be represented by this source and is
    // reported as unsupported instead of truncated.
    if (family == static_cast<unsigned int>(AF_PACKET)) {
        const ::sockaddr_ll* link =
            reinterpret_cast<const ::sockaddr_ll*>(row.ifa_addr);
        const std::size_t length =
            static_cast<std::size_t>(link->sll_halen);
        if (length > sizeof(link->sll_addr)) {
            return fail(errc::not_supported);
        }
        entry->hardware_address.assign(link->sll_addr,
                                       link->sll_addr + length);
        return {};
    }
#elif defined(AF_LINK)
    // Darwin and BSDs expose the link-layer address through an AF_LINK row. The
    // address begins after the interface name inside sdl_data and its
    // usable length is bounded by the recorded socket-address length.
    if (family == static_cast<unsigned int>(AF_LINK)) {
        const ::sockaddr_dl* link =
            reinterpret_cast<const ::sockaddr_dl*>(row.ifa_addr);
        const std::size_t name_length =
            static_cast<std::size_t>(link->sdl_nlen);
        const std::size_t address_length =
            static_cast<std::size_t>(link->sdl_alen);
        const std::size_t total_length =
            static_cast<std::size_t>(link->sdl_len);
        const std::size_t data_offset = offsetof(::sockaddr_dl, sdl_data);
        const std::size_t available =
            total_length > data_offset + name_length
                ? total_length - (data_offset + name_length)
                : 0U;
        if (address_length > available) {
            return fail(errc::malformed_data);
        }
        if (address_length > 0U) {
            const unsigned char* data =
                reinterpret_cast<const unsigned char*>(link) + data_offset +
                name_length;
            entry->hardware_address.assign(data, data + address_length);
        }
        return {};
    }
#endif

    // Families this slice does not represent are skipped, leaving room for
    // future slices without failing present-day enumeration.
    return {};
}

/// Converts a whole getifaddrs list into interface records.
template <typename InterfaceApi>
inline result<std::vector<network_common::interface_record>> convert_ifaddrs(
    ::ifaddrs* list) {
    std::vector<network_common::interface_record> interfaces;
    for (const ::ifaddrs* cursor = list; cursor != nullptr;
         cursor = cursor->ifa_next) {
        const result<void> converted =
            convert_ifaddrs_row<InterfaceApi>(*cursor, interfaces);
        if (!converted) { return fail(converted.error()); }
    }
    return interfaces;
}

/// Returns a snapshot of the platform's network interfaces and their
/// unicast addresses through the documented getifaddrs interface.
inline result<std::vector<network_common::interface_record>> interfaces() {
#if defined(__ANDROID__) && __ANDROID_API__ < 24
    return fail(errc::not_supported);
#else
    ::ifaddrs* list = nullptr;
    if (::getifaddrs(&list) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    const ifaddrs_guard guard(list);
    return convert_ifaddrs<native_interface_api>(list);
#endif
}

} // namespace network_backend
} // namespace detail
} // namespace syscape

#endif
