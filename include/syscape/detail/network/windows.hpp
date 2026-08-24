#ifndef SYSCAPE_DETAIL_NETWORK_WINDOWS_HPP
#define SYSCAPE_DETAIL_NETWORK_WINDOWS_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>

#include <syscape/detail/network/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace network_backend {

/// Converts UTF-16 interface text to UTF-8 at the platform boundary.
inline result<std::string> wide_to_utf8(std::wstring_view value) {
    static_assert(sizeof(wchar_t) == sizeof(char16_t),
                  "The Windows backend requires 16-bit wchar_t");
    std::u16string converted;
    converted.reserve(value.size());
    for (wchar_t unit : value) {
        converted.push_back(static_cast<char16_t>(unit));
    }
    return utf16_to_utf8(converted);
}

/// Owns a GetAdaptersAddresses table and releases it through the platform
/// API that produced it.
template <typename AdapterApi>
class adapter_table_guard {
public:
    explicit adapter_table_guard(::IP_ADAPTER_ADDRESSES* value) noexcept
        : value_(value) {}
    adapter_table_guard(const adapter_table_guard&) = delete;
    adapter_table_guard& operator=(const adapter_table_guard&) = delete;
    ~adapter_table_guard() {
        if (value_ != nullptr) { AdapterApi::release(value_); }
    }

    ::IP_ADAPTER_ADDRESSES* get() const noexcept { return value_; }

private:
    ::IP_ADAPTER_ADDRESSES* value_;
};

/// Platform calls used to enumerate network adapters.
///
/// The indirection exists so tests can drive enumeration with synthetic
/// adapter tables instead of real hardware; production callers always use
/// the native implementation.
struct native_adapter_api {
    /// Returns an owned GetAdaptersAddresses table for every adapter,
    /// growing the caller-allocated buffer while the platform reports
    /// overflow.
    ///
    /// The documented buffer-growth contract updates Size on overflow, so
    /// the loop converges; once the required size exceeds a fixed ceiling
    /// the loop stops and surfaces the native overflow code instead of
    /// growing without bound. ERROR_NO_DATA maps to not_found because the
    /// platform itself reports that no adapter information exists.
    static result<::IP_ADAPTER_ADDRESSES*> adapters() {
        constexpr ::ULONG initial_size = 15U * 1024U;
        constexpr ::ULONG maximum_size = 16U * 1024U * 1024U;
        ::ULONG size = initial_size;
        for (;;) {
            ::IP_ADAPTER_ADDRESSES* table =
                static_cast<::IP_ADAPTER_ADDRESSES*>(::malloc(size));
            if (table == nullptr) { return fail(errc::resource_exhausted); }
            const ::ULONG outcome =
                ::GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX,
                                       nullptr, table, &size);
            if (outcome == ERROR_SUCCESS) { return table; }
            ::free(table);
            if (outcome == ERROR_NO_DATA) {
                return fail(errc::not_found);
            }
            if (outcome == ERROR_BUFFER_OVERFLOW && size <= maximum_size) {
                continue;
            }
            return fail(std::error_code(static_cast<int>(outcome),
                                        std::system_category()));
        }
    }

    /// Releases a table obtained from adapters().
    static void release(::IP_ADAPTER_ADDRESSES* table) noexcept {
        ::free(table);
    }
};

/// Returns the portable state for a documented adapter operational status.
inline network_common::interface_state classify_oper_status(
    ::IF_OPER_STATUS status) noexcept {
    if (status == IfOperStatusUp) {
        return network_common::interface_state::up;
    }
    if (status == IfOperStatusDown) {
        return network_common::interface_state::down;
    }
    return network_common::interface_state::unknown;
}

/// Returns the portable name for one adapter row.
///
/// The friendly name is the operating system's user-facing interface label
/// and is reported as the interface name after UTF-8 conversion. Where an
/// adapter records none, the ANSI adapter identifier is used verbatim so
/// that every record still carries a usable name; an adapter with neither
/// is malformed platform data.
inline result<std::string> adapter_name(const ::IP_ADAPTER_ADDRESSES& row) {
    if (row.FriendlyName != nullptr && row.FriendlyName[0] != L'\0') {
        return wide_to_utf8(std::wstring_view(row.FriendlyName));
    }
    if (row.AdapterName == nullptr || row.AdapterName[0] == '\0') {
        return fail(errc::malformed_data);
    }
    return std::string(row.AdapterName);
}

/// Converts one unicast-address entry of an adapter row.
///
/// Address families this slice does not represent are skipped without
/// failing the query. A prefix length beyond its family's range is
/// malformed platform data because no documented producer emits one.
inline result<void> convert_unicast_entry(
    const ::IP_ADAPTER_UNICAST_ADDRESS& entry,
    network_common::interface_record& target) {
    if (entry.Address.lpSockaddr == nullptr ||
        entry.Address.iSockaddrLength <
            static_cast<int>(sizeof(::sockaddr))) {
        return fail(errc::malformed_data);
    }
    const unsigned int family =
        static_cast<unsigned int>(entry.Address.lpSockaddr->sa_family);

    network_common::unicast_record record;
    if (family == static_cast<unsigned int>(AF_INET)) {
        if (entry.Address.iSockaddrLength <
            static_cast<int>(sizeof(::sockaddr_in))) {
            return fail(errc::malformed_data);
        }
        const ::sockaddr_in* address =
            reinterpret_cast<const ::sockaddr_in*>(entry.Address.lpSockaddr);
        record.family = network_common::address_family::ipv4;
        const unsigned char* bytes =
            reinterpret_cast<const unsigned char*>(&address->sin_addr);
        for (std::size_t offset = 0U; offset < 4U; ++offset) {
            record.value[offset] = bytes[offset];
        }
    } else if (family == static_cast<unsigned int>(AF_INET6)) {
        if (entry.Address.iSockaddrLength <
            static_cast<int>(sizeof(::sockaddr_in6))) {
            return fail(errc::malformed_data);
        }
        const ::sockaddr_in6* address =
            reinterpret_cast<const ::sockaddr_in6*>(entry.Address.lpSockaddr);
        record.family = network_common::address_family::ipv6;
        const unsigned char* bytes =
            reinterpret_cast<const unsigned char*>(&address->sin6_addr);
        for (std::size_t offset = 0U; offset < 16U; ++offset) {
            record.value[offset] = bytes[offset];
        }
        record.scope_id = static_cast<std::uint32_t>(address->sin6_scope_id);
    } else {
        return {};
    }

    const std::uint64_t prefix =
        static_cast<std::uint64_t>(entry.OnLinkPrefixLength);
    const std::uint64_t maximum = static_cast<std::uint64_t>(
        network_common::maximum_prefix_length(record.family));
    if (prefix > maximum) { return fail(errc::malformed_data); }
    record.prefix_length = static_cast<std::uint8_t>(prefix);
    target.addresses.push_back(std::move(record));
    return {};
}

/// Converts one adapter row into the shared record shape.
inline result<network_common::interface_record> convert_adapter_row(
    const ::IP_ADAPTER_ADDRESSES& row) {
    result<std::string> name = adapter_name(row);
    if (!name) { return fail(name.error()); }

    network_common::interface_record record;
    record.name = std::move(*name);
    // IfIndex is documented as zero when IPv4 is unavailable. Preserve a
    // usable operating-system index for IPv6-only adapters instead of
    // rejecting that valid platform state as malformed data.
    record.index = static_cast<std::uint32_t>(
        row.IfIndex != 0U ? row.IfIndex : row.Ipv6IfIndex);
    if (record.index == 0U) { return fail(errc::not_supported); }
    record.state = classify_oper_status(row.OperStatus);
    record.loopback = row.IfType == IF_TYPE_SOFTWARE_LOOPBACK;
    record.mtu_bytes = static_cast<std::uint32_t>(row.Mtu);

    // The platform stores the address verbatim in a fixed-size buffer and
    // records how many bytes are meaningful; those bytes are copied without
    // reinterpretation, including none where the recorded length is zero.
    // A recorded length beyond the documented fixed buffer cannot be read
    // safely and is malformed platform data.
    if (row.PhysicalAddressLength >
        sizeof(row.PhysicalAddress) / sizeof(row.PhysicalAddress[0])) {
        return fail(errc::malformed_data);
    }
    record.hardware_address.assign(row.PhysicalAddress,
                                   row.PhysicalAddress +
                                       row.PhysicalAddressLength);

    for (const ::IP_ADAPTER_UNICAST_ADDRESS* entry = row.FirstUnicastAddress;
         entry != nullptr; entry = entry->Next) {
        const result<void> converted = convert_unicast_entry(*entry, record);
        if (!converted) { return fail(converted.error()); }
    }
    return record;
}

/// Converts a whole adapter table into interface records.
inline result<std::vector<network_common::interface_record>>
convert_adapter_table(const ::IP_ADAPTER_ADDRESSES* table) {
    std::vector<network_common::interface_record> interfaces;
    for (const ::IP_ADAPTER_ADDRESSES* cursor = table; cursor != nullptr;
         cursor = cursor->Next) {
        result<network_common::interface_record> record =
            convert_adapter_row(*cursor);
        if (!record) { return fail(record.error()); }
        interfaces.push_back(std::move(*record));
    }
    return interfaces;
}

/// Enumerates every adapter through the given platform API.
template <typename AdapterApi>
inline result<std::vector<network_common::interface_record>> enumerate(
    AdapterApi /*api*/) {
    const result<::IP_ADAPTER_ADDRESSES*> table = AdapterApi::adapters();
    if (!table) { return fail(table.error()); }
    const adapter_table_guard<AdapterApi> guard(*table);
    return convert_adapter_table(guard.get());
}

/// Returns a snapshot of the platform's network interfaces and their
/// unicast addresses through the documented GetAdaptersAddresses interface.
inline result<std::vector<network_common::interface_record>> interfaces() {
    return enumerate(native_adapter_api{});
}

template <typename RouteApi, typename Table>
class route_table_guard {
public:
    explicit route_table_guard(Table* value) noexcept
        : value_(value) {}
    route_table_guard(const route_table_guard&) = delete;
    route_table_guard& operator=(const route_table_guard&) = delete;
    ~route_table_guard() {
        if (value_ != nullptr) { RouteApi::release(value_); }
    }
    Table* get() const noexcept { return value_; }

private:
    Table* value_;
};

struct native_route_api {
    static result<::MIB_IPFORWARD_TABLE2*> table() {
        ::MIB_IPFORWARD_TABLE2* value = nullptr;
        const ::NETIO_STATUS outcome = ::GetIpForwardTable2(AF_UNSPEC, &value);
        if (outcome == ERROR_NOT_FOUND) { return fail(errc::not_found); }
        if (outcome != NO_ERROR) {
            return fail(std::error_code(static_cast<int>(outcome),
                                        std::system_category()));
        }
        if (value == nullptr) { return fail(errc::malformed_data); }
        return value;
    }
    static result<::MIB_UNICASTIPADDRESS_TABLE*> addresses() {
        ::MIB_UNICASTIPADDRESS_TABLE* value = nullptr;
        const ::NETIO_STATUS outcome =
            ::GetUnicastIpAddressTable(AF_UNSPEC, &value);
        if (outcome == ERROR_NOT_FOUND) { return fail(errc::not_found); }
        if (outcome != NO_ERROR) {
            return fail(std::error_code(static_cast<int>(outcome),
                                        std::system_category()));
        }
        if (value == nullptr) { return fail(errc::malformed_data); }
        return value;
    }
    static void release(::MIB_IPFORWARD_TABLE2* value) noexcept {
        ::FreeMibTable(value);
    }
    static void release(::MIB_UNICASTIPADDRESS_TABLE* value) noexcept {
        ::FreeMibTable(value);
    }
};

inline result<network_common::ip_address_record> convert_sockaddr_inet(
    const ::SOCKADDR_INET& source) {
    network_common::ip_address_record address;
    if (source.si_family == AF_INET) {
        address.family = network_common::address_family::ipv4;
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(
            &source.Ipv4.sin_addr);
        std::copy(bytes, bytes + 4U, address.value.begin());
    } else if (source.si_family == AF_INET6) {
        address.family = network_common::address_family::ipv6;
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(
            &source.Ipv6.sin6_addr);
        std::copy(bytes, bytes + 16U, address.value.begin());
        address.scope_id =
            static_cast<std::uint32_t>(source.Ipv6.sin6_scope_id);
    } else {
        return fail(errc::malformed_data);
    }
    return address;
}

inline bool windows_addresses_equal(
    const network_common::ip_address_record& left,
    const network_common::ip_address_record& right) noexcept {
    if (left.family != right.family) { return false; }
    const std::size_t size =
        left.family == network_common::address_family::ipv4 ? 4U : 16U;
    for (std::size_t offset = 0U; offset < size; ++offset) {
        if (left.value[offset] != right.value[offset]) { return false; }
    }
    return true;
}

inline bool windows_non_unicast_destination(
    const network_common::ip_address_record& destination,
    std::uint8_t prefix_length) noexcept {
    if (destination.family == network_common::address_family::ipv4) {
        if (destination.value[0U] == 127U ||
            destination.value[0U] >= 224U) {
            return true;
        }
        return prefix_length != 0U &&
               network_common::is_unspecified(destination);
    }
    const bool loopback =
        destination.value[0U] == 0U && destination.value[1U] == 0U &&
        destination.value[2U] == 0U && destination.value[3U] == 0U &&
        destination.value[4U] == 0U && destination.value[5U] == 0U &&
        destination.value[6U] == 0U && destination.value[7U] == 0U &&
        destination.value[8U] == 0U && destination.value[9U] == 0U &&
        destination.value[10U] == 0U && destination.value[11U] == 0U &&
        destination.value[12U] == 0U && destination.value[13U] == 0U &&
        destination.value[14U] == 0U && destination.value[15U] == 1U;
    return destination.value[0U] == 0xFFU || loopback ||
           (prefix_length != 0U &&
            network_common::is_unspecified(destination));
}

inline std::uint32_t windows_ipv4_value(
    const network_common::ip_address_record& address) noexcept {
    return (static_cast<std::uint32_t>(address.value[0U]) << 24U) |
           (static_cast<std::uint32_t>(address.value[1U]) << 16U) |
           (static_cast<std::uint32_t>(address.value[2U]) << 8U) |
           static_cast<std::uint32_t>(address.value[3U]);
}

inline result<bool> windows_local_or_broadcast_route(
    const network_common::ip_address_record& destination,
    std::uint8_t prefix_length, ::NET_IFINDEX interface_index,
    const ::MIB_UNICASTIPADDRESS_TABLE* addresses) {
    if (addresses == nullptr) { return false; }
    for (::ULONG offset = 0U; offset < addresses->NumEntries; ++offset) {
        const ::MIB_UNICASTIPADDRESS_ROW& row = addresses->Table[offset];
        if (row.InterfaceIndex != interface_index ||
            row.Address.si_family !=
                (destination.family == network_common::address_family::ipv4
                     ? AF_INET
                     : AF_INET6)) {
            continue;
        }
        const result<network_common::ip_address_record> local =
            convert_sockaddr_inet(row.Address);
        if (!local) { return fail(local.error()); }
        const std::uint8_t host_prefix =
            destination.family == network_common::address_family::ipv4
                ? std::uint8_t(32U)
                : std::uint8_t(128U);
        if (prefix_length == host_prefix &&
            windows_addresses_equal(destination, *local)) {
            return true;
        }
        if (destination.family == network_common::address_family::ipv4 &&
            prefix_length == 32U && row.OnLinkPrefixLength <= 30U) {
            const std::uint32_t host_mask =
                ~std::uint32_t(0U) >> row.OnLinkPrefixLength;
            if (windows_ipv4_value(destination) ==
                (windows_ipv4_value(*local) | host_mask)) {
                return true;
            }
        }
    }
    return false;
}

inline result<std::vector<network_common::route_record>>
convert_route_table(
    const ::MIB_IPFORWARD_TABLE2& table,
    const ::MIB_UNICASTIPADDRESS_TABLE* addresses = nullptr) {
    std::vector<network_common::route_record> routes;
    routes.reserve(static_cast<std::size_t>(table.NumEntries));
    for (::ULONG offset = 0U; offset < table.NumEntries; ++offset) {
        const ::MIB_IPFORWARD_ROW2& row = table.Table[offset];
        if (row.Loopback != FALSE) { continue; }
        result<network_common::ip_address_record> destination =
            convert_sockaddr_inet(row.DestinationPrefix.Prefix);
        if (!destination) { return fail(destination.error()); }
        const std::uint8_t prefix_length =
            static_cast<std::uint8_t>(row.DestinationPrefix.PrefixLength);
        const std::uint8_t maximum_prefix =
            network_common::maximum_prefix_length(destination->family);
        if (prefix_length > maximum_prefix) {
            return fail(errc::malformed_data);
        }
        if (windows_non_unicast_destination(*destination, prefix_length)) {
            continue;
        }
        const result<bool> local_or_broadcast =
            windows_local_or_broadcast_route(
                *destination, prefix_length, row.InterfaceIndex, addresses);
        if (!local_or_broadcast) { return fail(local_or_broadcast.error()); }
        if (*local_or_broadcast) { continue; }
        result<network_common::ip_address_record> next_hop =
            convert_sockaddr_inet(row.NextHop);
        if (!next_hop) { return fail(next_hop.error()); }
        network_common::route_record record;
        record.destination = *destination;
        record.prefix_length = prefix_length;
        if (!network_common::is_unspecified(*next_hop)) {
            record.next_hop = *next_hop;
        }
        record.interface_index =
            static_cast<std::uint32_t>(row.InterfaceIndex);
        record.metric = static_cast<std::uint32_t>(row.Metric);
        routes.push_back(std::move(record));
    }
    return routes;
}

template <typename RouteApi>
inline result<std::vector<network_common::route_record>> enumerate_routes(
    RouteApi /*api*/) {
    const result<::MIB_IPFORWARD_TABLE2*> table = RouteApi::table();
    if (!table) {
        if (table.error() == errc::not_found) {
            return std::vector<network_common::route_record> {};
        }
        return fail(table.error());
    }
    const route_table_guard<RouteApi, ::MIB_IPFORWARD_TABLE2> guard(*table);
    if (guard.get()->NumEntries == 0U) {
        return std::vector<network_common::route_record> {};
    }
    const result<::MIB_UNICASTIPADDRESS_TABLE*> addresses =
        RouteApi::addresses();
    if (!addresses) {
        if (addresses.error() == errc::not_found) {
            return convert_route_table(*guard.get());
        }
        return fail(addresses.error());
    }
    const route_table_guard<RouteApi, ::MIB_UNICASTIPADDRESS_TABLE>
        address_guard(*addresses);
    return convert_route_table(*guard.get(), address_guard.get());
}

inline result<std::vector<network_common::route_record>> routes() {
    return enumerate_routes(native_route_api {});
}

/// Platform calls used to read the system's global resolver identity.
///
/// The indirection exists so tests can drive loading with synthetic
/// records instead of the real platform; production callers always use
/// the native implementation.
struct native_dns_params_api {
    /// Returns an owned GetNetworkParams record, growing the
    /// caller-allocated buffer while the platform reports overflow.
    ///
    /// The documented buffer-growth contract updates Size on overflow, so
    /// the loop converges; once the required size exceeds a fixed ceiling
    /// the loop stops and surfaces the native overflow code instead of
    /// growing without bound.
    static result<::FIXED_INFO*> params() {
        constexpr ::ULONG maximum_size = 1024U * 1024U;
        ::ULONG size = sizeof(::FIXED_INFO);
        for (;;) {
            ::FIXED_INFO* value =
                static_cast<::FIXED_INFO*>(::malloc(size));
            if (value == nullptr) { return fail(errc::resource_exhausted); }
            const ::DWORD outcome = ::GetNetworkParams(value, &size);
            if (outcome == ERROR_SUCCESS) { return value; }
            ::free(value);
            if (outcome == ERROR_BUFFER_OVERFLOW && size <= maximum_size) {
                continue;
            }
            return fail(std::error_code(static_cast<int>(outcome),
                                        std::system_category()));
        }
    }

    /// Releases a record obtained from params().
    static void release(::FIXED_INFO* value) noexcept { ::free(value); }
};

/// Copies one fixed-size null-terminated ANSI field into bounded storage.
///
/// The documented fields are null-terminated character arrays; an array
/// without any terminator cannot be read safely and is malformed platform
/// data.
template <std::size_t Capacity>
inline result<std::string> copy_ansi_field(
    const char (&field)[Capacity]) {
    std::size_t end = 0U;
    while (end < Capacity && field[end] != '\0') { ++end; }
    if (end >= Capacity) { return fail(errc::malformed_data); }
    return std::string(field, end);
}

/// Converts one recorded resolver-server socket address.
///
/// Families this slice does not represent are skipped without failing the
/// query. A missing or truncated socket address is malformed platform data
/// because no safe reading exists.
inline result<std::optional<network_common::ip_address_record>>
convert_dns_server_address(const ::IP_ADAPTER_DNS_SERVER_ADDRESS& entry) {
    if (entry.Address.lpSockaddr == nullptr ||
        entry.Address.iSockaddrLength <
            static_cast<int>(sizeof(::sockaddr))) {
        return fail(errc::malformed_data);
    }
    const unsigned int family =
        static_cast<unsigned int>(entry.Address.lpSockaddr->sa_family);
    network_common::ip_address_record address;
    if (family == static_cast<unsigned int>(AF_INET)) {
        if (entry.Address.iSockaddrLength <
            static_cast<int>(sizeof(::sockaddr_in))) {
            return fail(errc::malformed_data);
        }
        const auto* socket_address =
            reinterpret_cast<const ::sockaddr_in*>(entry.Address.lpSockaddr);
        address.family = network_common::address_family::ipv4;
        const unsigned char* bytes =
            reinterpret_cast<const unsigned char*>(&socket_address->sin_addr);
        for (std::size_t offset = 0U; offset < 4U; ++offset) {
            address.value[offset] = bytes[offset];
        }
    } else if (family == static_cast<unsigned int>(AF_INET6)) {
        if (entry.Address.iSockaddrLength <
            static_cast<int>(sizeof(::sockaddr_in6))) {
            return fail(errc::malformed_data);
        }
        const auto* socket_address =
            reinterpret_cast<const ::sockaddr_in6*>(entry.Address.lpSockaddr);
        address.family = network_common::address_family::ipv6;
        const unsigned char* bytes =
            reinterpret_cast<const unsigned char*>(&socket_address->sin6_addr);
        for (std::size_t offset = 0U; offset < 16U; ++offset) {
            address.value[offset] = bytes[offset];
        }
        address.scope_id =
            static_cast<std::uint32_t>(socket_address->sin6_scope_id);
    } else {
        return std::optional<network_common::ip_address_record> {};
    }
    return std::optional<network_common::ip_address_record>(
        std::move(address));
}

/// Collects the platform DNS resolver configuration through the given
/// platform APIs.
///
/// Resolver servers come from the documented per-adapter
/// GetAdaptersAddresses chains concatenated in adapter enumeration order
/// and preserved verbatim within each adapter, including duplicates that
/// appear through several adapters. The local domain name comes from the
/// documented global DomainName field of GetNetworkParams. Neither source
/// exposes the distinct global suffix search list, so that optional field
/// remains unavailable instead of being fabricated from per-adapter suffixes.
template <typename AdapterApi, typename ParamsApi>
inline result<network_common::dns_record> collect_dns(
    AdapterApi /*adapter_api*/, ParamsApi /*params_api*/) {
    const result<::IP_ADAPTER_ADDRESSES*> table = AdapterApi::adapters();
    if (!table) { return fail(table.error()); }
    const adapter_table_guard<AdapterApi> guard(*table);

    network_common::dns_record collected;
    for (const ::IP_ADAPTER_ADDRESSES* cursor = guard.get();
         cursor != nullptr; cursor = cursor->Next) {
        // IfIndex is documented as zero when IPv4 is unavailable; fall
        // back to the IPv6 index so that a valid binding survives, and
        // leave the server unbound when neither exists.
        const std::uint32_t binding = static_cast<std::uint32_t>(
            cursor->IfIndex != 0U ? cursor->IfIndex : cursor->Ipv6IfIndex);
        const std::optional<std::uint32_t> bound_index =
            binding != 0U ? std::optional<std::uint32_t>(binding)
                          : std::nullopt;

        for (const ::IP_ADAPTER_DNS_SERVER_ADDRESS* server =
                 cursor->FirstDnsServerAddress;
             server != nullptr; server = server->Next) {
            result<std::optional<network_common::ip_address_record>>
                address = convert_dns_server_address(*server);
            if (!address) { return fail(address.error()); }
            if (!address->has_value()) { continue; }
            collected.servers.push_back(network_common::dns_server_record{
                **address, bound_index});
        }
    }

    const result<::FIXED_INFO*> parameters = ParamsApi::params();
    if (!parameters) { return fail(parameters.error()); }
    const route_table_guard<ParamsApi, ::FIXED_INFO> parameter_guard(
        *parameters);
    // The global domain name field is an ANSI rendering; validate it as
    // UTF-8 at the boundary so that an unusable encoding surfaces instead
    // of corrupted text.
    result<std::string> domain_name =
        copy_ansi_field(parameter_guard.get()->DomainName);
    if (!domain_name) { return fail(domain_name.error()); }
    if (!domain_name->empty()) {
        if (!is_valid_utf8(*domain_name)) {
            return fail(errc::invalid_encoding);
        }
        collected.domain_name = std::move(*domain_name);
    }
    return collected;
}

/// Returns a snapshot of the platform's DNS resolver configuration.
inline result<network_common::dns_record> dns() {
    return collect_dns(native_adapter_api{}, native_dns_params_api{});
}

} // namespace network_backend
} // namespace detail
} // namespace syscape

#endif
