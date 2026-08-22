#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <system_error>
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
                                               ::ULONG prefix) {
    ::IP_ADAPTER_UNICAST_ADDRESS entry {};
    std::memset(&storage, 0, sizeof(storage));
    storage.sin6_family = AF_INET6;
    std::memcpy(&storage.sin6_addr, bytes, 16U);
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
    std::memcpy(ethernet.PhysicalAddress, ethernet_address, 6U);
    ethernet.PhysicalAddressLength = 6U;
    ethernet.FirstUnicastAddress = &unicast;

    const unsigned char loopback_bytes[16] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
                                              0U, 0U, 0U, 0U, 0U, 0U, 0U,
                                              1U};
    ::sockaddr_in6 loopback_storage {};
    ::IP_ADAPTER_UNICAST_ADDRESS loopback_unicast = make_unicast_ipv6(
        loopback_storage, loopback_bytes, 128U);

    ::IP_ADAPTER_ADDRESSES loopback {};
    loopback.AdapterName = const_cast<::PSTR>("loopback-adapter");
    loopback.IfIndex = 1U;
    loopback.OperStatus = IfOperStatusDown;
    loopback.IfType = IF_TYPE_SOFTWARE_LOOPBACK;
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
               (*converted)[1U].addresses[0U].prefix_length == 128U,
           "An IPv6 unicast entry keeps its bytes and prefix");
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
        expect(!entry.name.empty(), "Every live adapter has a name");
        has_loopback = has_loopback || entry.loopback;
    }
    expect(has_loopback, "This host exposes a loopback adapter");
}

} // namespace

int main() {
    test_two_adapter_table();
    test_localized_friendly_name();
    test_unknown_oper_status();
    test_ipv6_only_index();
    test_adapter_without_protocol_index();
    test_unknown_unicast_family_skipped();
    test_prefix_and_socket_address_validation();
    test_nameless_adapter_is_malformed();
    test_enumeration_failure_preserved();
    test_empty_table_is_valid();
    test_live_enumeration();
    return failures == 0 ? 0 : 1;
}
