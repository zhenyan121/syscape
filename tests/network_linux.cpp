#include <cerrno>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iostream>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netpacket/packet.h>

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

    static void reset() {
        fail_resolution = false;
        next_index = 1U;
    }

    static syscape::result<std::uint32_t> index_of(const std::string&) {
        if (fail_resolution) {
            return syscape::fail(
                std::error_code(ENODEV, std::generic_category()));
        }
        return next_index++;
    }
};

bool fake_index_api::fail_resolution = false;
std::uint32_t fake_index_api::next_index = 1U;

/// Builds synthetic getifaddrs chains whose nodes stay address-stable.
class synthetic_chain {
public:
    ::ifaddrs* head() { return head_; }

    /// Adds a row without any socket address.
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

    /// Adds an IPv4 row wired to its address and netmask storage.
    ::ifaddrs& add_ipv4(const std::string& name, unsigned int flags,
                        std::uint32_t address_network_order,
                        std::uint32_t netmask_network_order) {
        ::ifaddrs& row = add(name, flags);
        ipv4_.emplace_back();
        std::memset(&ipv4_.back(), 0, sizeof(::sockaddr_in));
        ipv4_.back().sin_family = AF_INET;
        ipv4_.back().sin_addr.s_addr = address_network_order;
        masks_ipv4_.emplace_back();
        std::memset(&masks_ipv4_.back(), 0, sizeof(::sockaddr_in));
        masks_ipv4_.back().sin_family = AF_INET;
        masks_ipv4_.back().sin_addr.s_addr = netmask_network_order;
        row.ifa_addr = reinterpret_cast<::sockaddr*>(&ipv4_.back());
        row.ifa_netmask =
            reinterpret_cast<::sockaddr*>(&masks_ipv4_.back());
        return row;
    }

    /// Adds an IPv6 row wired to its address text and prefix-derived mask.
    ::ifaddrs& add_ipv6(const std::string& name, unsigned int flags,
                        const char* address_text, std::uint8_t prefix) {
        return add_ipv6_mask(name, flags, address_text,
                             prefix_mask6(prefix));
    }

    /// Adds an IPv6 row with an arbitrary raw netmask.
    ::ifaddrs& add_ipv6_mask(const std::string& name, unsigned int flags,
                             const char* address_text,
                             const std::array<unsigned char, 16>& mask) {
        ::ifaddrs& row = add(name, flags);
        ipv6_.emplace_back();
        std::memset(&ipv6_.back(), 0, sizeof(::sockaddr_in6));
        ipv6_.back().sin6_family = AF_INET6;
        if (::inet_pton(AF_INET6, address_text,
                        &ipv6_.back().sin6_addr) != 1) {
            std::cerr << "FAIL: invalid synthetic IPv6 text\n";
            ++failures;
        }
        masks_ipv6_.emplace_back();
        std::memset(&masks_ipv6_.back(), 0, sizeof(::sockaddr_in6));
        masks_ipv6_.back().sin6_family = AF_INET6;
        std::memcpy(&masks_ipv6_.back().sin6_addr, mask.data(), 16U);
        row.ifa_addr = reinterpret_cast<::sockaddr*>(&ipv6_.back());
        row.ifa_netmask =
            reinterpret_cast<::sockaddr*>(&masks_ipv6_.back());
        return row;
    }

    /// Adds an IPv4 address row whose recorded netmask has a foreign
    /// family, modeling inconsistent platform data.
    ::ifaddrs& add_ipv4_foreign_mask(const std::string& name,
                                     unsigned int flags,
                                     std::uint32_t address_network_order) {
        ::ifaddrs& row = add(name, flags);
        ipv4_.emplace_back();
        std::memset(&ipv4_.back(), 0, sizeof(::sockaddr_in));
        ipv4_.back().sin_family = AF_INET;
        ipv4_.back().sin_addr.s_addr = address_network_order;
        foreign_masks_.emplace_back();
        std::memset(&foreign_masks_.back(), 0, sizeof(::sockaddr_in6));
        foreign_masks_.back().sin6_family = AF_INET6;
        row.ifa_addr = reinterpret_cast<::sockaddr*>(&ipv4_.back());
        row.ifa_netmask =
            reinterpret_cast<::sockaddr*>(&foreign_masks_.back());
        return row;
    }

    /// Adds a link-layer row carrying length hardware bytes.
    ::ifaddrs& add_packet(const std::string& name, unsigned int flags,
                          unsigned char length,
                          const unsigned char* bytes) {
        ::ifaddrs& row = add(name, flags);
        packets_.emplace_back();
        std::memset(&packets_.back(), 0, sizeof(::sockaddr_ll));
        packets_.back().sll_family = AF_PACKET;
        packets_.back().sll_halen = length;
        if (bytes != nullptr && length > 0U) {
            std::memcpy(packets_.back().sll_addr, bytes,
                        length > sizeof(packets_.back().sll_addr)
                            ? sizeof(packets_.back().sll_addr)
                            : length);
        }
        row.ifa_addr = reinterpret_cast<::sockaddr*>(&packets_.back());
        return row;
    }

    /// Adds a row of a family this slice does not represent.
    ::ifaddrs& add_other_family(const std::string& name, unsigned int flags,
                                int family) {
        ::ifaddrs& row = add(name, flags);
        others_.emplace_back();
        std::memset(&others_.back(), 0, sizeof(::sockaddr));
        others_.back().sa_family = static_cast<sa_family_t>(family);
        row.ifa_addr = &others_.back();
        return row;
    }

private:
    static std::array<unsigned char, 16> prefix_mask6(std::uint8_t prefix) {
        std::array<unsigned char, 16> mask {};
        const std::size_t whole = static_cast<std::size_t>(prefix / 8U);
        const std::uint8_t rest = static_cast<std::uint8_t>(prefix % 8U);
        for (std::size_t offset = 0U; offset < whole && offset < 16U;
             ++offset) {
            mask[offset] = 0xFFU;
        }
        if (whole < 16U && rest != 0U) {
            mask[whole] = static_cast<unsigned char>(0xFFU << (8U - rest));
        }
        return mask;
    }

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
    std::deque<::sockaddr_in> ipv4_;
    std::deque<::sockaddr_in> masks_ipv4_;
    std::deque<::sockaddr_in6> ipv6_;
    std::deque<::sockaddr_in6> masks_ipv6_;
    std::deque<::sockaddr_in6> foreign_masks_;
    std::deque<::sockaddr_ll> packets_;
    std::deque<::sockaddr> others_;
    ::ifaddrs* head_ = nullptr;
    ::ifaddrs* tail_ = nullptr;
};

using syscape::detail::network_common::address_family;
using syscape::detail::network_common::interface_record;
using syscape::detail::network_common::interface_state;

void test_empty_list() {
    fake_index_api::reset();
    const auto converted =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            nullptr);
    expect(converted && converted->empty(),
           "An empty getifaddrs list converts to an empty snapshot");
}

void test_single_loopback() {
    fake_index_api::reset();
    fake_index_api::next_index = 7U;
    synthetic_chain chain;
    constexpr unsigned int loopback_flags =
        IFF_UP | IFF_RUNNING | IFF_LOOPBACK;
    chain.add("lo", loopback_flags);
    chain.add_ipv4("lo", loopback_flags, htonl(0x7F000001U),
                   htonl(0xFF000000U));

    const auto converted =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            chain.head());
    expect(converted && converted->size() == 1U,
           "Rows sharing one name merge into a single record");
    if (!converted || converted->empty()) { return; }
    const syscape::detail::network_common::interface_record& entry =
        (*converted)[0U];
    expect(entry.name == "lo",
           "The merged record keeps the interface name");
    expect(entry.index == 7U,
           "The injected resolver supplies the interface index");
    expect(entry.loopback,
           "The documented IFF_LOOPBACK flag classifies the interface as "
           "loopback");
    expect(entry.state == interface_state::up,
           "Up and running flags classify the interface as up");
    expect(entry.hardware_address.empty(),
           "A zero-length packet address is valid and stays empty");
    expect(entry.addresses.size() == 1U,
           "One IPv4 row yields exactly one unicast address");
    if (entry.addresses.size() != 1U) { return; }
    const syscape::detail::network_common::unicast_record& unicast =
        entry.addresses[0U];
    expect(unicast.family == address_family::ipv4,
           "An AF_INET row becomes an IPv4 record");
    expect(unicast.value[0U] == 127U && unicast.value[1U] == 0U &&
               unicast.value[2U] == 0U && unicast.value[3U] == 1U,
           "IPv4 octets stay in network byte order");
    bool tail_zero = true;
    for (std::size_t offset = 4U; offset < 16U; ++offset) {
        tail_zero = tail_zero && unicast.value[offset] == 0U;
    }
    expect(tail_zero, "Bytes beyond an IPv4 address remain zero");
    expect(unicast.prefix_length == 8U,
           "A contiguous 255.0.0.0 netmask becomes the prefix length 8");
}

void test_grouping_and_order() {
    fake_index_api::reset();
    synthetic_chain chain;
    chain.add_ipv4("eth0", IFF_UP | IFF_RUNNING, htonl(0xC0A80105U),
                   htonl(0xFFFFFF00U));
    chain.add_ipv6("wlan0", IFF_UP | IFF_RUNNING, "2001:db8::1", 64U);
    chain.add_ipv6("eth0", IFF_UP | IFF_RUNNING, "fe80::1", 64U);

    const auto converted =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            chain.head());
    expect(converted && converted->size() == 2U,
           "Interleaved rows group into per-interface records");
    if (!converted || converted->size() != 2U) { return; }
    expect((*converted)[0U].name == "eth0" &&
               (*converted)[1U].name == "wlan0",
           "Records keep the first-appearance order of the platform");
    expect((*converted)[0U].addresses.size() == 2U,
           "Later rows append their addresses to the existing record");
    expect((*converted)[0U].addresses[0U].prefix_length == 24U,
           "A contiguous 255.255.255.0 netmask becomes prefix length 24");
    expect((*converted)[0U].addresses[1U].family == address_family::ipv6 &&
               (*converted)[0U].addresses[1U].prefix_length == 64U,
           "An AF_INET6 row keeps its family and documented prefix");
    expect((*converted)[1U].addresses.size() == 1U &&
               (*converted)[1U].addresses[0U].value[0U] == 0x20U &&
               (*converted)[1U].addresses[0U].value[1U] == 0x01U &&
               (*converted)[1U].addresses[0U].value[2U] == 0x0DU &&
               (*converted)[1U].addresses[0U].value[3U] == 0xB8U,
           "IPv6 octets stay verbatim in network byte order");
}

void test_state_classification() {
    struct case_definition {
        unsigned int flags;
        interface_state expected;
        const char* message;
    };
    const case_definition cases[] = {
        {0U, interface_state::down,
         "An interface without IFF_UP is down"},
        {IFF_UP, interface_state::unknown,
         "Administratively up without running traffic is unknown"},
        {IFF_UP | IFF_RUNNING, interface_state::up,
         "Up and running flags classify the interface as up"}};
    for (const case_definition& item : cases) {
        expect(syscape::detail::network_backend::classify_state(
                   item.flags) == item.expected,
               item.message);
    }
}

void test_prefix_boundaries() {
    fake_index_api::reset();
    synthetic_chain chain;
    chain.add_ipv4("a", IFF_UP | IFF_RUNNING, htonl(0x0A000001U),
                   htonl(0xFFFFFFF0U));
    chain.add_ipv6("b", IFF_UP | IFF_RUNNING, "2001:db8::1", 128U);
    chain.add_ipv4("c", IFF_UP | IFF_RUNNING, htonl(0x0A000002U), 0U);

    const auto converted =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            chain.head());
    expect(converted && converted->size() == 3U,
           "Boundary-prefix rows convert without failure");
    if (!converted || converted->size() != 3U) { return; }
    expect((*converted)[0U].addresses[0U].prefix_length == 28U,
           "A 255.255.255.240 netmask becomes prefix length 28");
    expect((*converted)[1U].addresses[0U].prefix_length == 128U,
           "A full IPv6 netmask becomes prefix length 128");
    expect((*converted)[2U].addresses[0U].prefix_length == 0U,
           "A zero netmask legitimately yields prefix length 0");
}

void test_non_contiguous_mask_is_malformed() {
    fake_index_api::reset();
    synthetic_chain chain;
    chain.add_ipv4("eth0", IFF_UP | IFF_RUNNING, htonl(0x0A000001U),
                   htonl(0xFF00FF00U));

    const auto converted =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            chain.head());
    expect(!converted && converted.error() ==
                              syscape::make_error_code(
                                  syscape::errc::malformed_data),
           "A netmask with ones after zeros is malformed platform data");
}

void test_mask_family_mismatch_is_malformed() {
    fake_index_api::reset();
    synthetic_chain chain;
    chain.add_ipv4_foreign_mask("eth0", IFF_UP | IFF_RUNNING,
                                htonl(0x0A000001U));

    const auto converted =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            chain.head());
    expect(!converted &&
               converted.error() == syscape::make_error_code(
                                        syscape::errc::malformed_data),
           "An IPv4 address with a non-IPv4 netmask is malformed platform "
           "data");
}

void test_null_socket_address_contributes_flags_only() {
    fake_index_api::reset();
    synthetic_chain chain;
    chain.add("eth1", IFF_UP);
    chain.add_ipv4("eth0", IFF_UP | IFF_RUNNING, htonl(0x0A000001U),
                   htonl(0xFFFFFF00U));

    const auto converted =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            chain.head());
    expect(converted && converted->size() == 2U,
           "Rows without a socket address still create their record");
    if (!converted || converted->size() != 2U) { return; }
    expect((*converted)[0U].name == "eth1" &&
               (*converted)[0U].addresses.empty(),
           "A null socket address contributes no unicast address");
    expect((*converted)[0U].state == interface_state::unknown,
           "Flags-only rows still report their operational state");
}

void test_unknown_family_is_skipped() {
    fake_index_api::reset();
    synthetic_chain chain;
    chain.add_other_family("unix0", IFF_UP | IFF_RUNNING, AF_UNIX);

    const auto converted =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            chain.head());
    expect(converted && converted->size() == 1U &&
               (*converted)[0U].addresses.empty(),
           "Families this slice does not represent are skipped without "
           "failing enumeration");
}

void test_hardware_address_lengths() {
    fake_index_api::reset();
    const unsigned char six_bytes[] = {0xDEU, 0xADU, 0xBEU, 0xEFU, 0x01U,
                                       0x02U};

    synthetic_chain full;
    full.add_packet("eth0", IFF_UP | IFF_RUNNING, 6U, six_bytes);
    const auto converted_full =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            full.head());
    expect(converted_full &&
               converted_full->at(0U).hardware_address.size() == 6U &&
               converted_full->at(0U).hardware_address[0U] == 0xDEU &&
               converted_full->at(0U).hardware_address[5U] == 0x02U,
           "A six-byte packet address is copied verbatim");

    synthetic_chain empty;
    empty.add_packet("tun0", IFF_UP | IFF_RUNNING, 0U, nullptr);
    const auto converted_empty =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            empty.head());
    expect(converted_empty &&
               converted_empty->at(0U).hardware_address.empty(),
           "A zero-length hardware address stays empty instead of "
           "failing");

    synthetic_chain oversized;
    // A recorded hardware length beyond the documented packet storage
    // cannot be filled by the platform either; the row carries only the
    // declared length, which must be rejected instead of truncated.
    oversized.add_packet("ib0", IFF_UP | IFF_RUNNING, 20U, nullptr);
    const auto converted_over =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            oversized.head());
    expect(!converted_over &&
               converted_over.error() == syscape::make_error_code(
                                             syscape::errc::not_supported),
           "A link-layer length beyond the documented packet storage is "
           "reported unsupported instead of truncated");
}

void test_index_resolution_failure() {
    fake_index_api::reset();
    fake_index_api::fail_resolution = true;
    synthetic_chain chain;
    chain.add("eth0", IFF_UP | IFF_RUNNING);
    const auto converted =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            chain.head());
    expect(!converted &&
               converted.error() ==
                   std::error_code(ENODEV, std::generic_category()),
           "A failed interface-index resolution preserves its native "
           "error");
}

interface_record make_valid_record() {
    interface_record record;
    record.name = "eth0";
    record.index = 3U;
    return record;
}

void expect_validation_failure(
    interface_record record, syscape::errc expected, const char* message) {
    std::vector<interface_record> records;
    records.push_back(std::move(record));
    const auto outcome =
        syscape::detail::network_common::validate_interface_records(
            std::move(records));
    expect(!outcome &&
               outcome.error() == syscape::make_error_code(expected),
           message);
}

void test_boundary_validation() {
    using syscape::detail::network_common::unicast_record;

    {
        auto outcome = syscape::detail::network_common::
            validate_interface_records(std::vector<interface_record>{});
        expect(outcome.has_value(),
               "An empty interface snapshot is valid data");
    }

    {
        auto outcome = syscape::detail::network_common::
            validate_interface_records(std::vector<interface_record>{
                make_valid_record()});
        expect(outcome.has_value(),
               "A well-formed record passes boundary validation");
    }

    {
        interface_record record = make_valid_record();
        record.name.clear();
        expect_validation_failure(std::move(record),
                                  syscape::errc::malformed_data,
                                  "An empty interface name is malformed "
                                  "platform data");
    }

    {
        interface_record record = make_valid_record();
        record.index = 0U;
        expect_validation_failure(std::move(record),
                                  syscape::errc::malformed_data,
                                  "A zero interface index is malformed "
                                  "platform data");
    }

    {
        interface_record record = make_valid_record();
        record.name = "bad\xff";
        expect_validation_failure(std::move(record),
                                  syscape::errc::invalid_encoding,
                                  "Interface text must be valid UTF-8");
    }

    {
        interface_record record = make_valid_record();
        record.addresses.push_back(unicast_record{});
        record.addresses.back().family = address_family::ipv4;
        record.addresses.back().prefix_length = 33U;
        expect_validation_failure(std::move(record),
                                  syscape::errc::malformed_data,
                                  "An IPv4 prefix beyond 32 bits is "
                                  "malformed platform data");
    }

    {
        interface_record record = make_valid_record();
        record.addresses.push_back(unicast_record{});
        record.addresses.back().family = address_family::ipv6;
        record.addresses.back().prefix_length = 129U;
        expect_validation_failure(std::move(record),
                                  syscape::errc::malformed_data,
                                  "An IPv6 prefix beyond 128 bits is "
                                  "malformed platform data");
    }

    {
        interface_record record = make_valid_record();
        record.addresses.push_back(unicast_record{});
        record.addresses.back().family = address_family::ipv4;
        record.addresses.back().value[15U] = 1U;
        expect_validation_failure(std::move(record),
                                  syscape::errc::malformed_data,
                                  "Nonzero bytes beyond an IPv4 address "
                                  "are malformed platform data");
    }
}

std::set<std::string> collected_names(
    const std::vector<syscape::network::interface_entry>& entries) {
    std::set<std::string> names;
    for (const syscape::network::interface_entry& entry : entries) {
        names.insert(entry.name);
    }
    return names;
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
    std::size_t unicast_total = 0U;
    for (const syscape::network::interface_entry& entry : *interfaces) {
        expect(entry.index != 0U,
               "Every live interface has a nonzero index");
        expect(!entry.name.empty(), "Every live interface has a name");
        unicast_total += entry.addresses.size();
        for (const syscape::network::unicast_address& address :
             entry.addresses) {
            const bool ipv4 = address.family ==
                              syscape::network::address_family::ipv4;
            expect(address.prefix_length <= (ipv4 ? 32U : 128U),
                   "Every live prefix stays within its family range");
            if (ipv4) {
                expect(address.value[4U] == 0U,
                       "IPv4 records leave the extension bytes zero");
            }
        }
        if (entry.loopback) { has_loopback = true; }
    }
    expect(has_loopback, "This host exposes a loopback interface");

    bool loopback_address_present = false;
    for (const syscape::network::interface_entry& entry : *interfaces) {
        if (!entry.loopback) { continue; }
        for (const syscape::network::unicast_address& address :
             entry.addresses) {
            bool matches = false;
            if (address.family ==
                syscape::network::address_family::ipv4) {
                matches = address.value[0U] == 127U &&
                          address.value[1U] == 0U &&
                          address.value[2U] == 0U &&
                          address.value[3U] == 1U;
            } else if (address.family ==
                       syscape::network::address_family::ipv6) {
                matches = address.value[15U] == 1U;
                for (std::size_t offset = 0U; offset < 15U; ++offset) {
                    matches = matches && address.value[offset] == 0U;
                }
            }
            loopback_address_present = loopback_address_present || matches;
        }
    }
    expect(loopback_address_present,
           "The loopback interface carries its documented loopback "
           "address");

    // Cross-check against an independent getifaddrs walk over the same
    // live table.
    ::ifaddrs* list = nullptr;
    if (::getifaddrs(&list) != 0) { return; }
    std::set<std::string> independent_names;
    std::size_t independent_unicast = 0U;
    for (const ::ifaddrs* cursor = list; cursor != nullptr;
         cursor = cursor->ifa_next) {
        if (cursor->ifa_name != nullptr) {
            independent_names.insert(cursor->ifa_name);
        }
        if (cursor->ifa_addr != nullptr &&
            (cursor->ifa_addr->sa_family == AF_INET ||
             cursor->ifa_addr->sa_family == AF_INET6)) {
            ++independent_unicast;
        }
    }
    ::freeifaddrs(list);

    expect(collected_names(*interfaces) == independent_names,
           "The snapshot lists exactly the interfaces the platform does");
    expect(unicast_total == independent_unicast,
           "The snapshot counts every IPv4 and IPv6 unicast row");
}

} // namespace

int main() {
    test_empty_list();
    test_single_loopback();
    test_grouping_and_order();
    test_state_classification();
    test_prefix_boundaries();
    test_non_contiguous_mask_is_malformed();
    test_mask_family_mismatch_is_malformed();
    test_null_socket_address_contributes_flags_only();
    test_unknown_family_is_skipped();
    test_hardware_address_lengths();
    test_index_resolution_failure();
    test_boundary_validation();
    test_live_enumeration();
    return failures == 0 ? 0 : 1;
}
