#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>

#include <syscape/detail/network/common.hpp>
#include <syscape/detail/network/windows.hpp>
#include <syscape/network.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

/// Injected adapter enumeration shared by the synthetic conversions.
struct fake_adapter_api {
    static ::IP_ADAPTER_ADDRESSES* table;
    static bool fail_enumeration;
    static ::ULONG native_error;

    static syscape::result<::IP_ADAPTER_ADDRESSES*> adapters() {
        if (fail_enumeration) {
            return syscape::fail(std::error_code(
                static_cast<int>(native_error), std::system_category()));
        }
        return table;
    }

    static void release(::IP_ADAPTER_ADDRESSES*) noexcept {}
};

::IP_ADAPTER_ADDRESSES* fake_adapter_api::table = nullptr;
bool fake_adapter_api::fail_enumeration = false;
::ULONG fake_adapter_api::native_error = 0U;

struct fake_route_api {
    static ::MIB_IPFORWARD_TABLE2* value;
    static ::MIB_UNICASTIPADDRESS_TABLE* address_value;
    static bool released;
    static bool addresses_released;
    static bool no_routes;
    static bool no_addresses;
    static syscape::result<::MIB_IPFORWARD_TABLE2*> table() {
        if (no_routes) { return syscape::fail(syscape::errc::not_found); }
        return value;
    }
    static syscape::result<::MIB_UNICASTIPADDRESS_TABLE*> addresses() {
        if (no_addresses) { return syscape::fail(syscape::errc::not_found); }
        return address_value;
    }
    static void release(::MIB_IPFORWARD_TABLE2*) noexcept { released = true; }
    static void release(::MIB_UNICASTIPADDRESS_TABLE*) noexcept {
        addresses_released = true;
    }
};

::MIB_IPFORWARD_TABLE2* fake_route_api::value = nullptr;
::MIB_UNICASTIPADDRESS_TABLE* fake_route_api::address_value = nullptr;
bool fake_route_api::released = false;
bool fake_route_api::addresses_released = false;
bool fake_route_api::no_routes = false;
bool fake_route_api::no_addresses = false;

/// Converts one IPv4 address and prefix into wired unicast storage.
::IP_ADAPTER_UNICAST_ADDRESS make_unicast_ipv4(::sockaddr_in& storage,
                                               std::uint32_t address,
                                               ::ULONG prefix) {
    ::IP_ADAPTER_UNICAST_ADDRESS entry {};
    std::memset(&storage, 0, sizeof(storage));
    storage.sin_family = AF_INET;
    storage.sin_addr.s_addr = address;
    entry.Address.lpSockaddr = reinterpret_cast<::sockaddr*>(&storage);
    entry.Address.iSockaddrLength = static_cast<int>(sizeof(storage));
    entry.OnLinkPrefixLength = prefix;
    return entry;
}

::IP_ADAPTER_UNICAST_ADDRESS make_unicast_ipv6(::sockaddr_in6& storage,
                                               const unsigned char* bytes,
                                               ::ULONG prefix,
                                               std::uint32_t scope_id = 0U) {
    ::IP_ADAPTER_UNICAST_ADDRESS entry {};
    std::memset(&storage, 0, sizeof(storage));
    storage.sin6_family = AF_INET6;
    std::memcpy(&storage.sin6_addr, bytes, 16U);
    storage.sin6_scope_id = scope_id;
    entry.Address.lpSockaddr = reinterpret_cast<::sockaddr*>(&storage);
    entry.Address.iSockaddrLength = static_cast<int>(sizeof(storage));
    entry.OnLinkPrefixLength = prefix;
    return entry;
}

void test_two_adapter_table() {
    const wchar_t friendly[] = L"Ethernet";
    unsigned char ethernet_address[] = {0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU,
                                        0xFFU};

    ::sockaddr_in unicast_storage {};
    ::IP_ADAPTER_UNICAST_ADDRESS unicast = make_unicast_ipv4(
        unicast_storage, htonl(0xC0A80105U), 24U);

    ::IP_ADAPTER_ADDRESSES ethernet {};
    ethernet.FriendlyName = const_cast<::PWSTR>(friendly);
    ethernet.IfIndex = 5U;
    ethernet.OperStatus = IfOperStatusUp;
    ethernet.IfType = IF_TYPE_ETHERNET_CSMACD;
    ethernet.Mtu = 1500U;
    std::memcpy(ethernet.PhysicalAddress, ethernet_address, 6U);
    ethernet.PhysicalAddressLength = 6U;
    ethernet.FirstUnicastAddress = &unicast;

    const unsigned char loopback_bytes[16] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
                                              0U, 0U, 0U, 0U, 0U, 0U, 0U,
                                              1U};
    ::sockaddr_in6 loopback_storage {};
    ::IP_ADAPTER_UNICAST_ADDRESS loopback_unicast = make_unicast_ipv6(
        loopback_storage, loopback_bytes, 128U, 4U);

    ::IP_ADAPTER_ADDRESSES loopback {};
    loopback.AdapterName = const_cast<::PSTR>("loopback-adapter");
    loopback.IfIndex = 1U;
    loopback.OperStatus = IfOperStatusDown;
    loopback.IfType = IF_TYPE_SOFTWARE_LOOPBACK;
    loopback.Mtu = 65536U;
    loopback.FirstUnicastAddress = &loopback_unicast;

    ethernet.Next = &loopback;
    fake_adapter_api::table = &ethernet;
    fake_adapter_api::fail_enumeration = false;

    const auto converted =
        syscape::detail::network_backend::enumerate(fake_adapter_api{});
    expect(converted && converted->size() == 2U,
           "A two-adapter table converts into two records");
    if (!converted || converted->size() != 2U) { return; }

    expect((*converted)[0U].name == "Ethernet",
           "The friendly name becomes the interface name");
    expect((*converted)[0U].index == 5U,
           "The documented IfIndex supplies the interface index");
    expect((*converted)[0U].mtu_bytes == 1500U,
           "The documented adapter MTU is copied in bytes");
    expect((*converted)[0U].state ==
               syscape::detail::network_common::interface_state::up,
           "IfOperStatusUp classifies the adapter as up");
    expect(!(*converted)[0U].loopback,
           "An Ethernet adapter is not loopback");
    expect((*converted)[0U].hardware_address.size() == 6U &&
               (*converted)[0U].hardware_address[0U] == 0xAAU,
           "The physical address is copied verbatim");
    expect((*converted)[0U].addresses.size() == 1U &&
               (*converted)[0U].addresses[0U].prefix_length == 24U &&
               (*converted)[0U].addresses[0U].value[0U] == 192U,
           "An IPv4 unicast entry carries its documented prefix");

    expect((*converted)[1U].name == "loopback-adapter",
           "An adapter without a friendly name falls back to its ANSI "
           "identifier");
    expect((*converted)[1U].state ==
               syscape::detail::network_common::interface_state::down,
           "IfOperStatusDown classifies the adapter as down");
    expect((*converted)[1U].loopback,
           "IF_TYPE_SOFTWARE_LOOPBACK classifies the adapter as loopback");
    expect((*converted)[1U].hardware_address.empty(),
           "A zero physical-address length stays empty");
    expect((*converted)[1U].addresses.size() == 1U &&
               (*converted)[1U].addresses[0U].family ==
                   syscape::detail::network_common::address_family::ipv6 &&
               (*converted)[1U].addresses[0U].value[15U] == 1U &&
               (*converted)[1U].addresses[0U].prefix_length == 128U &&
               (*converted)[1U].addresses[0U].scope_id == 4U,
           "An IPv6 unicast entry keeps its bytes, prefix, and scope ID");
}

void test_localized_friendly_name() {
    const wchar_t localized[] = L"\x672C\x5730\x8FDE\x63A5";
    ::IP_ADAPTER_ADDRESSES adapter {};
    adapter.FriendlyName = const_cast<::PWSTR>(localized);
    adapter.IfIndex = 11U;
    adapter.OperStatus = IfOperStatusUp;
    adapter.IfType = IF_TYPE_ETHERNET_CSMACD;

    fake_adapter_api::table = &adapter;
    const auto converted =
        syscape::detail::network_backend::enumerate(fake_adapter_api{});
    expect(converted &&
               converted->at(0U).name ==
                   "\xE6\x9C\xAC\xE5\x9C\xB0\xE8\xBF\x9E\xE6\x8E\xA5",
           "A localized friendly name converts to UTF-8 at the boundary");
}

void test_unknown_oper_status() {
    ::IP_ADAPTER_ADDRESSES adapter {};
    adapter.AdapterName = const_cast<::PSTR>("testing-adapter");
    adapter.IfIndex = 12U;
    adapter.OperStatus = IfOperStatusTesting;
    adapter.IfType = IF_TYPE_ETHERNET_CSMACD;

    fake_adapter_api::table = &adapter;
    const auto converted =
        syscape::detail::network_backend::enumerate(fake_adapter_api{});
    expect(converted &&
               converted->at(0U).state ==
                   syscape::detail::network_common::interface_state::
                       unknown,
           "An operational status this slice does not map is unknown");
}

void test_ipv6_only_index() {
    ::IP_ADAPTER_ADDRESSES adapter {};
    adapter.AdapterName = const_cast<::PSTR>("ipv6-only-adapter");
    adapter.IfIndex = 0U;
    adapter.Ipv6IfIndex = 27U;
    adapter.OperStatus = IfOperStatusUp;

    fake_adapter_api::table = &adapter;
    const auto converted =
        syscape::detail::network_backend::enumerate(fake_adapter_api{});
    expect(converted && converted->at(0U).index == 27U,
           "An IPv6-only adapter uses its documented IPv6 interface index");
}

void test_adapter_without_protocol_index() {
    ::IP_ADAPTER_ADDRESSES adapter {};
    adapter.AdapterName = const_cast<::PSTR>("unindexed-adapter");
    adapter.IfIndex = 0U;
    adapter.Ipv6IfIndex = 0U;

    fake_adapter_api::table = &adapter;
    const auto converted =
        syscape::detail::network_backend::enumerate(fake_adapter_api{});
    expect(!converted &&
               converted.error() ==
                   syscape::make_error_code(syscape::errc::not_supported),
           "An adapter without either protocol index is an explicit "
           "unsupported representation, not malformed data");
}

void test_zero_mtu_is_malformed_at_boundary() {
    ::IP_ADAPTER_ADDRESSES adapter {};
    adapter.AdapterName = const_cast<::PSTR>("zero-mtu-adapter");
    adapter.IfIndex = 29U;
    adapter.Mtu = 0U;
    fake_adapter_api::table = &adapter;
    auto converted =
        syscape::detail::network_backend::enumerate(fake_adapter_api{});
    const auto validated = syscape::detail::network_common::
        validate_interface_records(std::move(converted));
    expect(!validated &&
               validated.error() == syscape::make_error_code(
                                        syscape::errc::malformed_data),
           "A zero adapter MTU is malformed at the public boundary");
}

void test_unknown_unicast_family_skipped() {
    ::sockaddr storage {};
    storage.sa_family = AF_UNIX;
    ::IP_ADAPTER_UNICAST_ADDRESS unicast {};
    unicast.Address.lpSockaddr = &storage;
    unicast.Address.iSockaddrLength = static_cast<int>(sizeof(storage));
    unicast.OnLinkPrefixLength = 64U;

    ::IP_ADAPTER_ADDRESSES adapter {};
    adapter.AdapterName = const_cast<::PSTR>("mixed-adapter");
    adapter.IfIndex = 13U;
    adapter.OperStatus = IfOperStatusUp;
    adapter.FirstUnicastAddress = &unicast;

    fake_adapter_api::table = &adapter;
    const auto converted =
        syscape::detail::network_backend::enumerate(fake_adapter_api{});
    expect(converted && converted->at(0U).addresses.empty(),
           "Unicast entries of unrepresented families are skipped");
}

void test_prefix_and_socket_address_validation() {
    ::sockaddr_in wide_storage {};
    ::IP_ADAPTER_UNICAST_ADDRESS wide_prefix = make_unicast_ipv4(
        wide_storage, htonl(0x0A000001U), 33U);
    ::IP_ADAPTER_ADDRESSES wide {};
    wide.AdapterName = const_cast<::PSTR>("wide-adapter");
    wide.IfIndex = 14U;
    wide.FirstUnicastAddress = &wide_prefix;
    fake_adapter_api::table = &wide;
    {
        const auto converted =
            syscape::detail::network_backend::enumerate(
                fake_adapter_api{});
        expect(!converted &&
                   converted.error() == syscape::make_error_code(
                                            syscape::errc::malformed_data),
               "An IPv4 prefix beyond 32 bits is malformed platform data");
    }

    ::IP_ADAPTER_UNICAST_ADDRESS without_socket {};
    without_socket.Address.lpSockaddr = nullptr;
    ::IP_ADAPTER_ADDRESSES missing {};
    missing.AdapterName = const_cast<::PSTR>("missing-adapter");
    missing.IfIndex = 15U;
    missing.FirstUnicastAddress = &without_socket;
    fake_adapter_api::table = &missing;
    {
        const auto converted =
            syscape::detail::network_backend::enumerate(
                fake_adapter_api{});
        expect(!converted &&
                   converted.error() == syscape::make_error_code(
                                            syscape::errc::malformed_data),
               "A unicast entry without a socket address is malformed "
               "platform data");
    }

    ::sockaddr_in short_storage {};
    short_storage.sin_family = AF_INET;
    ::IP_ADAPTER_UNICAST_ADDRESS short_socket {};
    short_socket.Address.lpSockaddr =
        reinterpret_cast<::sockaddr*>(&short_storage);
    short_socket.Address.iSockaddrLength =
        static_cast<int>(sizeof(::sockaddr_in) - 1U);
    ::IP_ADAPTER_ADDRESSES truncated {};
    truncated.AdapterName = const_cast<::PSTR>("truncated-adapter");
    truncated.IfIndex = 18U;
    truncated.FirstUnicastAddress = &short_socket;
    fake_adapter_api::table = &truncated;
    {
        const auto converted =
            syscape::detail::network_backend::enumerate(
                fake_adapter_api{});
        expect(!converted &&
                   converted.error() == syscape::make_error_code(
                                            syscape::errc::malformed_data),
               "A socket address shorter than its family structure is "
               "malformed platform data");
    }

    ::IP_ADAPTER_ADDRESSES oversized_address {};
    oversized_address.AdapterName =
        const_cast<::PSTR>("oversized-adapter");
    oversized_address.IfIndex = 17U;
    oversized_address.PhysicalAddressLength =
        sizeof(oversized_address.PhysicalAddress) + 1U;
    fake_adapter_api::table = &oversized_address;
    {
        const auto converted =
            syscape::detail::network_backend::enumerate(
                fake_adapter_api{});
        expect(!converted &&
                   converted.error() == syscape::make_error_code(
                                            syscape::errc::malformed_data),
               "A physical-address length beyond the documented buffer is "
               "malformed platform data");
    }
}

void test_nameless_adapter_is_malformed() {
    ::IP_ADAPTER_ADDRESSES nameless {};
    nameless.IfIndex = 16U;
    fake_adapter_api::table = &nameless;
    const auto converted =
        syscape::detail::network_backend::enumerate(fake_adapter_api{});
    expect(!converted && converted.error() ==
                              syscape::make_error_code(
                                  syscape::errc::malformed_data),
           "An adapter with neither name field is malformed platform "
           "data");
}

void test_enumeration_failure_preserved() {
    fake_adapter_api::fail_enumeration = true;
    fake_adapter_api::native_error = 31U;
    const auto converted =
        syscape::detail::network_backend::enumerate(fake_adapter_api{});
    expect(!converted &&
               converted.error() ==
                   std::error_code(31, std::system_category()),
           "A failed adapter enumeration preserves its native error");
    fake_adapter_api::fail_enumeration = false;
}

void test_empty_table_is_valid() {
    fake_adapter_api::table = nullptr;
    const auto converted =
        syscape::detail::network_backend::enumerate(fake_adapter_api{});
    expect(converted && converted->empty(),
           "An empty adapter table is valid data");
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
           "A running hosted system exposes at least one adapter");
    bool has_loopback = false;
    for (const syscape::network::interface_entry& entry : *interfaces) {
        expect(entry.index != 0U,
               "Every live adapter has a nonzero index");
        expect(entry.mtu_bytes != 0U,
               "Every live adapter has a nonzero MTU");
        expect(!entry.name.empty(), "Every live adapter has a name");
        has_loopback = has_loopback || entry.loopback;
    }
    expect(has_loopback, "This host exposes a loopback adapter");
}

void test_route_table_conversion_and_release() {
    ::MIB_IPFORWARD_TABLE2 table {};
    table.NumEntries = 1U;
    ::MIB_IPFORWARD_ROW2& row = table.Table[0U];
    row.DestinationPrefix.Prefix.Ipv4.sin_family = AF_INET;
    row.DestinationPrefix.PrefixLength = 0U;
    row.NextHop.Ipv4.sin_family = AF_INET;
    const unsigned char gateway[4] = {10U, 0U, 0U, 1U};
    std::memcpy(&row.NextHop.Ipv4.sin_addr, gateway, sizeof(gateway));
    row.InterfaceIndex = 8U;
    row.Metric = 42U;
    ::MIB_UNICASTIPADDRESS_TABLE addresses {};
    fake_route_api::value = &table;
    fake_route_api::address_value = &addresses;
    fake_route_api::released = false;
    fake_route_api::addresses_released = false;
    fake_route_api::no_routes = false;
    fake_route_api::no_addresses = false;
    const auto converted =
        syscape::detail::network_backend::enumerate_routes(fake_route_api {});
    expect(converted && converted->size() == 1U &&
               converted->at(0U).next_hop &&
               converted->at(0U).next_hop->value[0U] == 10U &&
               converted->at(0U).interface_index == 8U &&
               converted->at(0U).metric &&
               *converted->at(0U).metric == 42U,
           "A Windows default route keeps its gateway, interface, and metric");
    expect(fake_route_api::released,
           "The Windows route table is released on successful conversion");
    expect(fake_route_api::addresses_released,
           "The Windows unicast-address table is released after filtering");

    row.Loopback = TRUE;
    const auto skipped =
        syscape::detail::network_backend::convert_route_table(table);
    expect(skipped && skipped->empty(),
           "Windows loopback routes are omitted from forwarding routes");

    fake_route_api::no_routes = true;
    const auto empty =
        syscape::detail::network_backend::enumerate_routes(fake_route_api {});
    expect(empty && empty->empty(),
           "Windows reports a missing route table as a valid empty snapshot");
    fake_route_api::no_routes = false;

    table.NumEntries = 0U;
    fake_route_api::released = false;
    fake_route_api::addresses_released = false;
    const auto zero_rows =
        syscape::detail::network_backend::enumerate_routes(fake_route_api {});
    expect(zero_rows && zero_rows->empty() && fake_route_api::released &&
               !fake_route_api::addresses_released,
           "An empty Windows route table needs no address-table query");
}

void set_windows_ipv4_destination(::MIB_IPFORWARD_ROW2& row,
                                  const unsigned char (&bytes)[4],
                                  unsigned char prefix_length) {
    row.DestinationPrefix.Prefix.Ipv4.sin_family = AF_INET;
    std::memcpy(&row.DestinationPrefix.Prefix.Ipv4.sin_addr, bytes,
                sizeof(bytes));
    row.DestinationPrefix.PrefixLength = prefix_length;
}

void test_windows_non_forwarding_routes_are_filtered() {
    ::MIB_IPFORWARD_TABLE2 table {};
    table.NumEntries = 1U;
    ::MIB_IPFORWARD_ROW2& row = table.Table[0U];
    row.InterfaceIndex = 8U;
    row.NextHop.Ipv4.sin_family = AF_INET;

    ::MIB_UNICASTIPADDRESS_TABLE addresses {};
    addresses.NumEntries = 1U;
    ::MIB_UNICASTIPADDRESS_ROW& local = addresses.Table[0U];
    local.InterfaceIndex = 8U;
    local.Address.Ipv4.sin_family = AF_INET;
    const unsigned char local_bytes[4] = {10U, 0U, 0U, 5U};
    std::memcpy(&local.Address.Ipv4.sin_addr, local_bytes,
                sizeof(local_bytes));
    local.OnLinkPrefixLength = 24U;

    auto expect_filtered = [&](const unsigned char (&destination)[4],
                               unsigned char prefix_length,
                               const char* message) {
        set_windows_ipv4_destination(row, destination, prefix_length);
        const auto converted =
            syscape::detail::network_backend::convert_route_table(
                table, &addresses);
        expect(converted && converted->empty(), message);
    };

    expect_filtered(local_bytes, 32U,
                    "Windows local host routes are omitted");
    const unsigned char directed_broadcast[4] = {10U, 0U, 0U, 255U};
    expect_filtered(directed_broadcast, 32U,
                    "Windows directed-broadcast routes are omitted");
    const unsigned char limited_broadcast[4] = {255U, 255U, 255U, 255U};
    expect_filtered(limited_broadcast, 32U,
                    "Windows limited-broadcast routes are omitted");
    const unsigned char multicast[4] = {224U, 0U, 0U, 0U};
    expect_filtered(multicast, 4U, "Windows multicast routes are omitted");

    row.DestinationPrefix.Prefix.Ipv6.sin6_family = AF_INET6;
    reinterpret_cast<unsigned char*>(
        &row.DestinationPrefix.Prefix.Ipv6.sin6_addr)[0U] = 0xFFU;
    row.DestinationPrefix.PrefixLength = 8U;
    row.NextHop.Ipv6.sin6_family = AF_INET6;
    const auto ipv6 =
        syscape::detail::network_backend::convert_route_table(table,
                                                               &addresses);
    expect(ipv6 && ipv6->empty(), "Windows IPv6 multicast routes are omitted");
}

/// Injected global resolver identity shared by the DNS tests.
struct fake_params_api {
    static ::FIXED_INFO value;
    static bool fail_call;
    static ::DWORD native_error;

    static syscape::result<::FIXED_INFO*> params() {
        if (fail_call) {
            return syscape::fail(std::error_code(
                static_cast<int>(native_error), std::system_category()));
        }
        return &value;
    }

    static void release(::FIXED_INFO*) noexcept {}
};

::FIXED_INFO fake_params_api::value {};
bool fake_params_api::fail_call = false;
::DWORD fake_params_api::native_error = 0U;

/// Builds one synthetic per-adapter resolver-server entry.
::IP_ADAPTER_DNS_SERVER_ADDRESS make_dns_server_storage(
    ::SOCKADDR_INET& storage, const unsigned char* bytes,
    std::size_t size, std::uint16_t family, std::uint32_t scope_id = 0U) {
    std::memset(&storage, 0, sizeof(storage));
    int length = 0;
    if (family == AF_INET) {
        storage.Ipv4.sin_family = AF_INET;
        std::memcpy(&storage.Ipv4.sin_addr, bytes, size);
        length = static_cast<int>(sizeof(::sockaddr_in));
    } else if (family == AF_INET6) {
        storage.Ipv6.sin6_family = AF_INET6;
        std::memcpy(&storage.Ipv6.sin6_addr, bytes, size);
        storage.Ipv6.sin6_scope_id = scope_id;
        length = static_cast<int>(sizeof(::sockaddr_in6));
    } else {
        storage.Ipv4.sin_family = family;
        length = static_cast<int>(sizeof(::sockaddr));
    }
    ::IP_ADAPTER_DNS_SERVER_ADDRESS entry {};
    entry.Address.lpSockaddr =
        reinterpret_cast<::sockaddr*>(&storage);
    entry.Address.iSockaddrLength = length;
    return entry;
}

void test_windows_dns_collection_order_and_bindings() {
    const unsigned char first_ipv4[4] = {192U, 168U, 1U, 53U};
    const unsigned char link_local[16] = {0xFEU, 0x80U, 0U, 0U, 0U, 0U, 0U,
                                          0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
                                          1U};
    const unsigned char duplicate_ipv4[4] = {10U, 0U, 0U, 53U};

    ::SOCKADDR_INET server_one_storage {};
    ::IP_ADAPTER_DNS_SERVER_ADDRESS server_one = make_dns_server_storage(
        server_one_storage, first_ipv4, sizeof(first_ipv4), AF_INET);
    ::SOCKADDR_INET server_two_storage {};
    ::IP_ADAPTER_DNS_SERVER_ADDRESS server_two = make_dns_server_storage(
        server_two_storage, link_local, sizeof(link_local), AF_INET6, 5U);
    ::SOCKADDR_INET server_three_storage {};
    ::IP_ADAPTER_DNS_SERVER_ADDRESS server_three = make_dns_server_storage(
        server_three_storage, duplicate_ipv4, sizeof(duplicate_ipv4),
        AF_INET);

    server_one.Next = &server_two;

    ::IP_ADAPTER_ADDRESSES first {};
    first.IfIndex = 5U;
    first.FirstDnsServerAddress = &server_one;

    ::IP_ADAPTER_ADDRESSES second {};
    second.IfIndex = 0U;
    second.Ipv6IfIndex = 7U;
    second.FirstDnsServerAddress = &server_three;

    first.Next = &second;
    fake_adapter_api::table = &first;
    fake_adapter_api::fail_enumeration = false;
    std::strcpy(fake_params_api::value.DomainName, "corp.example.com");
    fake_params_api::fail_call = false;

    const auto collected = syscape::detail::network_backend::collect_dns(
        fake_adapter_api{}, fake_params_api{});
    expect(collected && collected->servers.size() == 3U &&
               !collected->search_domains &&
               collected->domain_name &&
               *collected->domain_name == "corp.example.com",
           "Windows DNS records concatenate in adapter enumeration order");
    if (!collected) { return; }

    using syscape::detail::network_common::address_family;
    expect(collected->servers[0U].address.family ==
                   address_family::ipv4 &&
               collected->servers[0U].interface_index &&
               *collected->servers[0U].interface_index == 5U,
           "The first adapter's IPv4 resolver keeps its binding");
    expect(collected->servers[1U].address.family ==
                   address_family::ipv6 &&
               collected->servers[1U].address.scope_id == 5U &&
               collected->servers[1U].interface_index &&
               *collected->servers[1U].interface_index == 5U,
           "A link-local resolver keeps its zone and adapter binding");
    expect(collected->servers[2U].interface_index &&
               *collected->servers[2U].interface_index == 7U &&
               collected->servers[2U].address.value[0U] == 10U,
           "An IPv6-only adapter binds through its documented fallback");
    expect(!collected->search_domains,
           "The unavailable global suffix list is not fabricated from "
           "per-adapter suffixes");
}

void test_windows_dns_edge_cases() {
    const unsigned char server_ipv4[4] = {10U, 0U, 0U, 53U};
    ::SOCKADDR_INET unknown_family_storage {};
    ::IP_ADAPTER_DNS_SERVER_ADDRESS unknown_family =
        make_dns_server_storage(unknown_family_storage, server_ipv4,
                                sizeof(server_ipv4), AF_UNSPEC);
    unknown_family.Address.iSockaddrLength =
        static_cast<int>(sizeof(::sockaddr));

    ::IP_ADAPTER_ADDRESSES row {};
    row.IfIndex = 0U;
    row.Ipv6IfIndex = 0U;
    row.FirstDnsServerAddress = &unknown_family;
    fake_adapter_api::table = &row;
    fake_adapter_api::fail_enumeration = false;
    fake_params_api::value.DomainName[0U] = '\0';
    fake_params_api::fail_call = false;

    const auto unbound = syscape::detail::network_backend::collect_dns(
        fake_adapter_api{}, fake_params_api{});
    expect(unbound && unbound->servers.empty() &&
               !unbound->search_domains && !unbound->domain_name,
           "Unrepresented families and empty fields stay valid data");

    const unsigned char truncated_bytes[4] = {10U, 0U, 0U, 54U};
    ::SOCKADDR_INET truncated_storage {};
    ::IP_ADAPTER_DNS_SERVER_ADDRESS truncated = make_dns_server_storage(
        truncated_storage, truncated_bytes, sizeof(truncated_bytes),
        AF_INET6, 3U);
    truncated.Address.iSockaddrLength =
        static_cast<int>(sizeof(::sockaddr));
    row.FirstDnsServerAddress = &truncated;
    const auto short_in6 =
        syscape::detail::network_backend::collect_dns(
            fake_adapter_api{}, fake_params_api{});
    expect(!short_in6 &&
               short_in6.error() == syscape::errc::malformed_data,
           "A truncated IPv6 resolver record is malformed");

    ::SOCKADDR_INET missing_storage {};
    ::IP_ADAPTER_DNS_SERVER_ADDRESS missing = make_dns_server_storage(
        missing_storage, truncated_bytes, sizeof(truncated_bytes), AF_INET);
    missing.Address.lpSockaddr = nullptr;
    row.FirstDnsServerAddress = &missing;
    const auto missing_address =
        syscape::detail::network_backend::collect_dns(
            fake_adapter_api{}, fake_params_api{});
    expect(!missing_address &&
               missing_address.error() == syscape::errc::malformed_data,
           "A missing resolver socket address is malformed");

    row.FirstDnsServerAddress = nullptr;
    std::memset(fake_params_api::value.DomainName, 0xABU,
                sizeof(fake_params_api::value.DomainName));
    const auto unterminated =
        syscape::detail::network_backend::collect_dns(
            fake_adapter_api{}, fake_params_api{});
    expect(!unterminated &&
               unterminated.error() == syscape::errc::malformed_data,
           "An unterminated domain-name field is malformed");

    std::strcpy(fake_params_api::value.DomainName, "bad\xff domain");
    const auto invalid_encoding =
        syscape::detail::network_backend::collect_dns(
            fake_adapter_api{}, fake_params_api{});
    expect(!invalid_encoding &&
               invalid_encoding.error() == syscape::errc::invalid_encoding,
           "A non-UTF-8 domain name reports an encoding failure");

    std::strcpy(fake_params_api::value.DomainName, "corp.example.com");
    fake_params_api::fail_call = true;
    fake_params_api::native_error = ERROR_ACCESS_DENIED;
    const auto native_failure =
        syscape::detail::network_backend::collect_dns(
            fake_adapter_api{}, fake_params_api{});
    expect(!native_failure &&
               native_failure.error().value() == ERROR_ACCESS_DENIED &&
               native_failure.error().category() ==
                   std::system_category(),
           "A failing global identity call preserves its native error");
    fake_params_api::fail_call = false;
}

struct fake_if_table_api {
    static ::PMIB_IF_TABLE2 table_value;
    static bool fail_call;
    static ::DWORD native_error;

    static syscape::result<::PMIB_IF_TABLE2> table() {
        if (fail_call) {
            return syscape::fail(std::error_code(
                static_cast<int>(native_error), std::system_category()));
        }
        return table_value;
    }

    static void release(::PMIB_IF_TABLE2) noexcept {}
};

::PMIB_IF_TABLE2 fake_if_table_api::table_value = nullptr;
bool fake_if_table_api::fail_call = false;
::DWORD fake_if_table_api::native_error = 0U;

void test_windows_statistics_conversion() {
    alignas(::MIB_IF_TABLE2) unsigned char storage[sizeof(::MIB_IF_TABLE2) + sizeof(::MIB_IF_ROW2)];
    std::memset(storage, 0, sizeof(storage));
    ::PMIB_IF_TABLE2 table = reinterpret_cast<::PMIB_IF_TABLE2>(storage);
    table->NumEntries = 1U;
    ::MIB_IF_ROW2& row = table->Table[0];
    row.InterfaceIndex = 42U;
    std::wcscpy(row.Alias, L"Ethernet 1");
    row.InOctets = 1000ULL;
    row.OutOctets = 2000ULL;
    row.InUcastPkts = 10ULL;
    row.InNUcastPkts = 2ULL;
    row.OutUcastPkts = 20ULL;
    row.OutNUcastPkts = 3ULL;
    row.InErrors = 1ULL;
    row.OutErrors = 2ULL;
    row.InDiscards = 3ULL;
    row.OutDiscards = 4ULL;

    fake_if_table_api::table_value = table;
    fake_if_table_api::fail_call = false;

    fake_if_table_api api;
    const auto collected =
        syscape::detail::network_backend::collect_statistics(api);
    expect(collected.has_value(), "Windows statistics collect successfully");
    if (collected) {
        expect(collected->size() == 1U, "One statistics entry collected");
        const auto& rec = (*collected)[0];
        expect(rec.name == "Ethernet 1", "Friendly alias name converted");
        expect(rec.index == 42U, "Interface index matches");
        expect(rec.rx_bytes == 1000ULL, "rx_bytes match");
        expect(rec.tx_bytes == 2000ULL, "tx_bytes match");
        expect(rec.rx_packets == 12ULL, "rx_packets match");
        expect(rec.tx_packets == 23ULL, "tx_packets match");
        expect(rec.rx_errors == 1ULL, "rx_errors match");
        expect(rec.tx_errors == 2ULL, "tx_errors match");
        expect(rec.rx_dropped == 3ULL, "rx_dropped match");
        expect(rec.tx_dropped == 4ULL, "tx_dropped match");
        expect(!rec.rx_multicast,
               "Windows leaves unavailable multicast packet count empty");
    }

    row.InUcastPkts = (std::numeric_limits<::ULONG64>::max)();
    row.InNUcastPkts = 1ULL;
    const auto overflow =
        syscape::detail::network_backend::convert_if_row(row);
    expect(!overflow && overflow.error() == syscape::errc::value_too_large,
           "Combined Windows packet counters reject overflow");

    fake_if_table_api::table_value = nullptr;
    const auto null_table =
        syscape::detail::network_backend::collect_statistics(
            fake_if_table_api{});
    expect(!null_table && null_table.error() == syscape::errc::malformed_data,
           "A successful Windows table call cannot return a null table");
}

} // namespace

int main() {
    test_two_adapter_table();
    test_localized_friendly_name();
    test_unknown_oper_status();
    test_ipv6_only_index();
    test_adapter_without_protocol_index();
    test_zero_mtu_is_malformed_at_boundary();
    test_unknown_unicast_family_skipped();
    test_prefix_and_socket_address_validation();
    test_nameless_adapter_is_malformed();
    test_enumeration_failure_preserved();
    test_empty_table_is_valid();
    test_live_enumeration();
    test_route_table_conversion_and_release();
    test_windows_non_forwarding_routes_are_filtered();
    test_windows_dns_collection_order_and_bindings();
    test_windows_dns_edge_cases();
    test_windows_statistics_conversion();
    return failures == 0 ? 0 : 1;
}
