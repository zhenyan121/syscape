#include <cerrno>
#include <cstdint>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <unistd.h>

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
    static bool fail_mtu;
    static std::uint32_t next_index;
    static std::uint32_t mtu_bytes;
    static std::uint32_t mtu_queries;

    static void reset() {
        fail_resolution = false;
        fail_mtu = false;
        next_index = 1U;
        mtu_bytes = 1500U;
        mtu_queries = 0U;
    }

    static syscape::result<std::uint32_t> index_of(const std::string&) {
        if (fail_resolution) {
            return syscape::fail(
                std::error_code(ENODEV, std::generic_category()));
        }
        return next_index++;
    }

    static syscape::result<std::uint32_t> mtu_of(const std::string&) {
        ++mtu_queries;
        if (fail_mtu) {
            return syscape::fail(
                std::error_code(EACCES, std::generic_category()));
        }
        return mtu_bytes;
    }
};

bool fake_index_api::fail_resolution = false;
bool fake_index_api::fail_mtu = false;
std::uint32_t fake_index_api::next_index = 1U;
std::uint32_t fake_index_api::mtu_bytes = 1500U;
std::uint32_t fake_index_api::mtu_queries = 0U;

struct interrupting_mtu_ioctl_api {
    static std::uint32_t calls;

    static int get(int, ::ifreq* request) noexcept {
        ++calls;
        if (calls == 1U) {
            errno = EINTR;
            return -1;
        }
        request->ifr_mtu = 9000;
        return 0;
    }
};

std::uint32_t interrupting_mtu_ioctl_api::calls = 0U;

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
                        const char* address_text, std::uint8_t prefix,
                        std::uint32_t scope_id = 0U) {
        return add_ipv6_mask(name, flags, address_text,
                             prefix_mask6(prefix), scope_id);
    }

    /// Adds an IPv6 row with an arbitrary raw netmask.
    ::ifaddrs& add_ipv6_mask(const std::string& name, unsigned int flags,
                             const char* address_text,
                             const std::array<unsigned char, 16>& mask,
                             std::uint32_t scope_id = 0U) {
        ::ifaddrs& row = add(name, flags);
        ipv6_.emplace_back();
        std::memset(&ipv6_.back(), 0, sizeof(::sockaddr_in6));
        ipv6_.back().sin6_family = AF_INET6;
        ipv6_.back().sin6_scope_id = scope_id;
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
    expect(entry.mtu_bytes == 1500U,
           "The injected MTU query supplies the interface MTU");
    expect(fake_index_api::mtu_queries == 1U,
           "Rows sharing an interface query its MTU only once");
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
    chain.add_ipv6("eth0", IFF_UP | IFF_RUNNING, "fe80::1", 64U, 9U);

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
               (*converted)[0U].addresses[1U].prefix_length == 64U &&
               (*converted)[0U].addresses[1U].scope_id == 9U,
           "An AF_INET6 row keeps its prefix and numeric scope identifier");
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

void test_mtu_failure_is_preserved() {
    fake_index_api::reset();
    fake_index_api::fail_mtu = true;
    synthetic_chain chain;
    chain.add("eth0", IFF_UP | IFF_RUNNING);
    const auto converted =
        syscape::detail::network_backend::convert_ifaddrs<fake_index_api>(
            chain.head());
    expect(!converted &&
               converted.error() ==
                   std::error_code(EACCES, std::generic_category()),
           "A failed MTU lookup preserves its native error");
}

void test_interrupted_mtu_ioctl_is_retried() {
    interrupting_mtu_ioctl_api::calls = 0U;
    ::ifreq request {};
    const auto mtu = syscape::detail::network_backend::
        read_mtu<interrupting_mtu_ioctl_api>(-1, request);
    expect(mtu && *mtu == 9000U &&
               interrupting_mtu_ioctl_api::calls == 2U,
           "An interrupted MTU ioctl is retried before conversion");
}

interface_record make_valid_record() {
    interface_record record;
    record.name = "eth0";
    record.index = 3U;
    record.mtu_bytes = 1500U;
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
        record.mtu_bytes = 0U;
        expect_validation_failure(std::move(record),
                                  syscape::errc::malformed_data,
                                  "A zero MTU is malformed platform data");
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
        record.addresses.back().family = address_family::ipv4;
        record.addresses.back().scope_id = 2U;
        expect_validation_failure(std::move(record),
                                  syscape::errc::malformed_data,
                                  "An IPv4 scope identifier is malformed "
                                  "platform data");
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
        expect(entry.mtu_bytes != 0U,
               "Every live interface has a nonzero MTU");
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

    const int descriptor = ::socket(AF_INET, SOCK_DGRAM, 0);
    expect(descriptor >= 0,
           "A datagram socket is available for the independent MTU check");
    if (descriptor >= 0) {
        for (const syscape::network::interface_entry& entry : *interfaces) {
            ::ifreq request {};
            if (entry.name.size() >= static_cast<std::size_t>(IFNAMSIZ)) {
                expect(false,
                       "A live interface name fits the documented ioctl "
                       "storage");
                continue;
            }
            std::memcpy(request.ifr_name, entry.name.data(),
                        entry.name.size());
            const int outcome = ::ioctl(descriptor, SIOCGIFMTU, &request);
            expect(outcome == 0,
                   "The independent ioctl resolves each live MTU");
            if (outcome == 0) {
                expect(request.ifr_mtu > 0 &&
                           static_cast<std::uint32_t>(request.ifr_mtu) ==
                               entry.mtu_bytes,
                       "The snapshot MTU matches an independent ioctl");
            }
        }
        ::close(descriptor);
    }

    // Cross-check against an independent getifaddrs walk over the same
    // live table.
    ::ifaddrs* list = nullptr;
    if (::getifaddrs(&list) != 0) { return; }
    std::set<std::string> independent_names;
    std::size_t independent_unicast = 0U;
    bool scope_ids_match = true;
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
        if (cursor->ifa_name != nullptr && cursor->ifa_addr != nullptr &&
            cursor->ifa_addr->sa_family == AF_INET6) {
            const ::sockaddr_in6* native =
                reinterpret_cast<const ::sockaddr_in6*>(cursor->ifa_addr);
            bool found = false;
            for (const syscape::network::interface_entry& entry :
                 *interfaces) {
                if (entry.name != cursor->ifa_name) { continue; }
                for (const syscape::network::unicast_address& address :
                     entry.addresses) {
                    if (address.family == address_family::ipv6 &&
                        std::memcmp(address.value.data(), &native->sin6_addr,
                                    16U) == 0 &&
                        address.scope_id == native->sin6_scope_id) {
                        found = true;
                    }
                }
            }
            scope_ids_match = scope_ids_match && found;
        }
    }
    ::freeifaddrs(list);

    expect(collected_names(*interfaces) == independent_names,
           "The snapshot lists exactly the interfaces the platform does");
    expect(unicast_total == independent_unicast,
           "The snapshot counts every IPv4 and IPv6 unicast row");
    expect(scope_ids_match,
           "IPv6 scope identifiers match an independent getifaddrs walk");
}

void test_route_boundary_validation() {
    using syscape::detail::network_common::ip_address_record;
    using syscape::detail::network_common::route_record;
    route_record valid;
    valid.destination.family = address_family::ipv4;
    valid.destination.value[0U] = 192U;
    valid.destination.value[1U] = 168U;
    valid.destination.value[2U] = 1U;
    valid.prefix_length = 24U;
    valid.interface_index = 2U;
    auto accepted = syscape::detail::network_common::validate_route_records(
        std::vector<route_record> {valid});
    expect(accepted.has_value(), "A canonical route passes validation");

    route_record host_bits = valid;
    host_bits.destination.value[3U] = 1U;
    auto rejected = syscape::detail::network_common::validate_route_records(
        std::vector<route_record> {host_bits});
    expect(!rejected && rejected.error() == syscape::errc::malformed_data,
           "A destination with host bits set is malformed");

    route_record mismatched = valid;
    ip_address_record gateway;
    gateway.family = address_family::ipv6;
    gateway.value[15U] = 1U;
    mismatched.next_hop = gateway;
    rejected = syscape::detail::network_common::validate_route_records(
        std::vector<route_record> {mismatched});
    expect(!rejected && rejected.error() == syscape::errc::malformed_data,
           "A next hop from another address family is malformed");

    route_record unindexed = valid;
    unindexed.interface_index = 0U;
    rejected = syscape::detail::network_common::validate_route_records(
        std::vector<route_record> {unindexed});
    expect(!rejected && rejected.error() == syscape::errc::malformed_data,
           "A zero route interface index is malformed");
}

void append_route_attribute(std::vector<unsigned char>& message,
                            std::uint16_t type, const void* value,
                            std::size_t size) {
    ::rtattr attribute {};
    attribute.rta_type = type;
    attribute.rta_len = static_cast<std::uint16_t>(sizeof(attribute) + size);
    const std::size_t offset = message.size();
    const std::size_t recorded = static_cast<std::size_t>(attribute.rta_len);
    message.resize(offset +
                   syscape::detail::network_backend::netlink_align(recorded),
                   0U);
    std::memcpy(message.data() + offset, &attribute, sizeof(attribute));
    if (size != 0U) {
        std::memcpy(message.data() + offset + sizeof(attribute), value, size);
    }
}

void test_linux_route_message_conversion() {
    ::rtmsg header {};
    header.rtm_family = AF_INET;
    header.rtm_dst_len = 0U;
    header.rtm_type = RTN_UNICAST;
    std::vector<unsigned char> message(sizeof(header));
    std::memcpy(message.data(), &header, sizeof(header));
    const unsigned char gateway[4] = {192U, 168U, 1U, 1U};
    const std::uint32_t interface_index = 7U;
    const std::uint32_t metric = 25U;
    append_route_attribute(message, RTA_GATEWAY, gateway, sizeof(gateway));
    append_route_attribute(message, RTA_OIF, &interface_index,
                           sizeof(interface_index));
    append_route_attribute(message, RTA_PRIORITY, &metric, sizeof(metric));
    std::vector<syscape::detail::network_common::route_record> routes;
    const auto converted =
        syscape::detail::network_backend::append_linux_route_message(
            message.data(), message.size(), routes);
    expect(converted && routes.size() == 1U,
           "A Linux default route converts into one record");
    if (!converted || routes.empty()) { return; }
    expect(routes[0U].prefix_length == 0U &&
               routes[0U].interface_index == 7U &&
               routes[0U].next_hop &&
               routes[0U].next_hop->value[3U] == 1U &&
               routes[0U].metric && *routes[0U].metric == 25U,
           "The Linux route keeps its gateway, interface, and metric");

    message.pop_back();
    routes.clear();
    const auto truncated =
        syscape::detail::network_backend::append_linux_route_message(
            message.data(), message.size(), routes);
    expect(!truncated && truncated.error() == syscape::errc::malformed_data,
           "A truncated Linux route attribute is malformed");
}

void append_multipath_hop(std::vector<unsigned char>& multipath,
                          std::int32_t interface_index,
                          const unsigned char* gateway,
                          std::size_t gateway_size) {
    const std::size_t offset = multipath.size();
    multipath.resize(offset + sizeof(::rtnexthop), 0U);
    append_route_attribute(multipath, RTA_GATEWAY, gateway, gateway_size);
    ::rtnexthop hop {};
    hop.rtnh_len = static_cast<unsigned short>(multipath.size() - offset);
    hop.rtnh_ifindex = interface_index;
    std::memcpy(multipath.data() + offset, &hop, sizeof(hop));
}

void test_linux_multipath_and_ipv6_scope() {
    ::rtmsg header {};
    header.rtm_family = AF_INET;
    header.rtm_type = RTN_UNICAST;
    std::vector<unsigned char> message(sizeof(header));
    std::memcpy(message.data(), &header, sizeof(header));
    std::vector<unsigned char> multipath;
    const unsigned char first[4] = {10U, 0U, 0U, 1U};
    const unsigned char second[4] = {10U, 0U, 1U, 1U};
    append_multipath_hop(multipath, 3, first, sizeof(first));
    append_multipath_hop(multipath, 4, second, sizeof(second));
    append_route_attribute(message, RTA_MULTIPATH, multipath.data(),
                           multipath.size());
    std::vector<syscape::detail::network_common::route_record> routes;
    auto converted =
        syscape::detail::network_backend::append_linux_route_message(
            message.data(), message.size(), routes);
    expect(converted && routes.size() == 2U &&
               routes[0U].interface_index == 3U &&
               routes[1U].interface_index == 4U &&
               routes[1U].next_hop && routes[1U].next_hop->value[2U] == 1U,
           "A Linux multipath route expands in next-hop order");

    header.rtm_family = AF_INET6;
    message.assign(sizeof(header), 0U);
    std::memcpy(message.data(), &header, sizeof(header));
    unsigned char link_local[16] {};
    link_local[0U] = 0xFEU;
    link_local[1U] = 0x80U;
    link_local[15U] = 1U;
    const std::uint32_t interface_index = 9U;
    append_route_attribute(message, RTA_GATEWAY, link_local,
                           sizeof(link_local));
    append_route_attribute(message, RTA_OIF, &interface_index,
                           sizeof(interface_index));
    routes.clear();
    converted = syscape::detail::network_backend::append_linux_route_message(
        message.data(), message.size(), routes);
    expect(converted && routes.size() == 1U && routes[0U].next_hop &&
               routes[0U].next_hop->scope_id == 9U,
           "A link-local Linux IPv6 gateway uses its output-interface scope");
}

void test_linux_nexthop_object_route() {
    ::rtmsg header {};
    header.rtm_family = AF_INET;
    header.rtm_type = RTN_UNICAST;
    std::vector<unsigned char> message(sizeof(header));
    std::memcpy(message.data(), &header, sizeof(header));
    const std::uint32_t nexthop_id = 17U;
    append_route_attribute(
        message, syscape::detail::network_backend::linux_rta_nh_id,
        &nexthop_id, sizeof(nexthop_id));

    std::vector<syscape::detail::network_common::route_record> routes;
    auto converted =
        syscape::detail::network_backend::append_linux_route_message(
            message.data(), message.size(), routes);
    expect(!converted && converted.error() == syscape::errc::not_supported,
           "A compact Linux nexthop-object route is unsupported, not malformed");

    const std::uint32_t interface_index = 12U;
    append_route_attribute(message, RTA_OIF, &interface_index,
                           sizeof(interface_index));
    converted = syscape::detail::network_backend::append_linux_route_message(
        message.data(), message.size(), routes);
    expect(converted && routes.size() == 1U &&
               routes[0U].interface_index == interface_index,
           "A compatibility-expanded nexthop-object route still converts");
}

void test_live_routes() {
    const auto listed = syscape::network::routes();
    if (!listed) {
        expect(listed.error() == std::errc::permission_denied ||
                   listed.error() == std::errc::operation_not_permitted ||
                   listed.error() == std::errc::operation_not_supported,
               "Live routes fail only when access is denied or unsupported");
        return;
    }
    const auto interfaces = syscape::network::interfaces();
    expect(interfaces.has_value(),
           "Interfaces are available when the live route table is available");
    std::set<std::uint32_t> indices;
    if (interfaces) {
        for (const auto& entry : *interfaces) { indices.insert(entry.index); }
    }
    std::size_t expected_gateway_count = 0U;
    for (const auto& route : *listed) {
        expect(route.interface_index != 0U,
               "Every live route has a nonzero interface index");
        if (interfaces) {
            expect(indices.count(route.interface_index) != 0U,
                   "Every live route refers to an enumerated interface");
        }
        if (route.prefix_length == 0U && route.next_hop) {
            ++expected_gateway_count;
            expect(route.destination.value[0U] == 0U,
                   "A default route has an unspecified destination");
        }
    }
    const auto gateways = syscape::network::default_gateways();
    expect(gateways && gateways->size() == expected_gateway_count,
           "Default gateways are exactly explicit next hops from /0 routes");
}

/// Injected resolver-configuration reader shared by the DNS tests.
struct fake_resolver_reader {
    static std::string content;
    static bool succeed;
    static std::error_code failure;
    static bool portable_too_large;

    static syscape::result<std::string> read() {
        if (!succeed) {
            if (portable_too_large) {
                return syscape::fail(syscape::errc::value_too_large);
            }
            return syscape::fail(failure);
        }
        return content;
    }
};

std::string fake_resolver_reader::content;
bool fake_resolver_reader::succeed = true;
std::error_code fake_resolver_reader::failure;
bool fake_resolver_reader::portable_too_large = false;

/// Injected zone resolver shared by the scoped-name-server tests.
struct fake_dns_index_api {
    static bool fail_resolution;
    static std::uint32_t next_index;

    static void reset() {
        fail_resolution = false;
        next_index = 7U;
    }

    static syscape::result<std::uint32_t> index_of(const std::string&) {
        if (fail_resolution) {
            return syscape::fail(
                std::error_code(ENXIO, std::generic_category()));
        }
        return next_index++;
    }
};

bool fake_dns_index_api::fail_resolution = false;
std::uint32_t fake_dns_index_api::next_index = 7U;

void test_dns_parses_documented_file() {
    const auto parsed =
        syscape::detail::network_backend::
            parse_resolver_conf<fake_dns_index_api>(
        "# leading comment\n"
        "; also a comment\n"
        "nameserver 192.168.1.1\n"
        "\t nameserver\t2001:db8::53\t\n"
        "\r\n"
        "options edns0 trust-ad\r\n"
        "sortlist 10.0.0.0/8 192.0.2.0/24\n"
        "unknown-directive whatever value\n"
        "   # indented comment line\n"
        "search example.net example.com\n");
    expect(parsed && parsed->servers.size() == 2U &&
               parsed->search_domains &&
               parsed->search_domains->size() == 2U &&
               (*parsed->search_domains)[0U] == "example.net" &&
               (*parsed->search_domains)[1U] == "example.com" &&
               !parsed->domain_name.has_value(),
           "Comments, options, sortlist, and unknown lines are skipped");
    if (!parsed || parsed->servers.size() != 2U) { return; }

    using syscape::detail::network_common::address_family;
    expect((*parsed).servers[0U].address.family == address_family::ipv4 &&
               (*parsed).servers[0U].address.value[0U] == 192U &&
               (*parsed).servers[0U].address.value[3U] == 1U &&
               !(*parsed).servers[0U].interface_index,
           "The first Linux resolver keeps its IPv4 bytes and no binding");
    expect((*parsed).servers[1U].address.family == address_family::ipv6 &&
               (*parsed).servers[1U].address.value[0U] == 0x20U &&
               (*parsed).servers[1U].address.value[1U] == 0x01U &&
               (*parsed).servers[1U].address.value[2U] == 0x0DU &&
               (*parsed).servers[1U].address.value[3U] == 0xB8U &&
               (*parsed).servers[1U].address.value[15U] == 0x53U,
           "The second Linux resolver keeps its IPv6 bytes verbatim");

    const auto empty = syscape::detail::network_backend::
        parse_resolver_conf<fake_dns_index_api>(
            "# nothing configured\n\n");
    expect(empty && empty->servers.empty() &&
               empty->search_domains && empty->search_domains->empty() &&
               !empty->domain_name,
           "An empty configuration is valid data with no facts");
}

void test_dns_domain_directive_is_single_entry_list() {
    const auto parsed =
        syscape::detail::network_backend::
            parse_resolver_conf<fake_dns_index_api>(
                "domain internal.example.org\n");
    expect(parsed && parsed->servers.empty() &&
               parsed->search_domains &&
               parsed->search_domains->size() == 1U &&
               (*parsed->search_domains)[0U] == "internal.example.org" &&
               parsed->domain_name &&
               *parsed->domain_name == "internal.example.org",
           "A domain directive records one list entry and the local domain");
}

void test_dns_search_and_domain_last_wins() {
    const auto search_wins =
        syscape::detail::network_backend::
            parse_resolver_conf<fake_dns_index_api>(
                "domain stale.example.org\n"
                "search fresh.example.org other.example.org\n");
    expect(search_wins &&
               search_wins->search_domains &&
               search_wins->search_domains->size() == 2U &&
               (*search_wins->search_domains)[0U] == "fresh.example.org" &&
               !search_wins->domain_name.has_value(),
           "A later search directive overrides an earlier domain");

    const auto domain_wins =
        syscape::detail::network_backend::
            parse_resolver_conf<fake_dns_index_api>(
                "search a.example.org b.example.org\n"
                "domain final.example.org\n");
    expect(domain_wins &&
               domain_wins->search_domains &&
               domain_wins->search_domains->size() == 1U &&
               (*domain_wins->search_domains)[0U] == "final.example.org" &&
               domain_wins->domain_name &&
               *domain_wins->domain_name == "final.example.org",
           "A later domain directive overrides an earlier search list");

    const auto repeated_search =
        syscape::detail::network_backend::
            parse_resolver_conf<fake_dns_index_api>(
                "nameserver 10.0.0.1\n"
                "nameserver 10.0.0.2\n"
                "search old.example.org\n"
                "search new.example.org extra.example.org\n");
    expect(repeated_search && repeated_search->servers.size() == 2U &&
               repeated_search->search_domains &&
               repeated_search->search_domains->size() == 2U &&
               (*repeated_search->search_domains)[0U] == "new.example.org",
           "Later directives replace earlier instances of the same kind");
}

void test_dns_scoped_nameserver_values() {
    fake_dns_index_api::reset();
    const auto resolved =
        syscape::detail::network_backend::
            parse_resolver_conf<fake_dns_index_api>(
                "nameserver fe80::1%wlo1\n");
    expect(resolved && resolved->servers.size() == 1U &&
               resolved->servers[0U].address.scope_id == 7U,
           "A de-facto scoped resolver records its numeric zone");

    const std::vector<std::string> malformed = {
        "nameserver fe80::1%\n",
        "nameserver fe80::1%wlo1%x\n",
        "nameserver 192.168.1.1%eth0\n",
        "nameserver fe80::1%" + std::string(IF_NAMESIZE, 'x') + "\n"};
    for (const std::string& text : malformed) {
        const auto parsed =
            syscape::detail::network_backend::
                parse_resolver_conf<fake_dns_index_api>(text);
        expect(!parsed &&
                   parsed.error() == syscape::errc::malformed_data,
               "An unusable scoped nameserver record is malformed data");
    }

    fake_dns_index_api::reset();
    fake_dns_index_api::fail_resolution = true;
    const auto vanished =
        syscape::detail::network_backend::
            parse_resolver_conf<fake_dns_index_api>(
                "nameserver fe80::1%gone0\n"
                "nameserver 10.0.0.9\n");
    expect(vanished && vanished->servers.size() == 1U &&
               vanished->servers[0U].address.value[0U] == 10U,
           "A zone naming a vanished interface skips only its own entry");
    fake_dns_index_api::fail_resolution = false;
}

void test_dns_malformed_records_fail() {
    fake_dns_index_api::reset();
    const char* malformed[] = {
        "nameserver not-an-address\n",
        "nameserver 999.168.1.1\n",
        "nameserver 192.168.001.001\n",
        "nameserver 192.168.1.1 extra-token\n",
        "nameserver 192.168.1.1 # trailing comment text is not documented\n",
        "nameserver\n",
        "domain one.example.org two.example.org\n",
        "domain\n",
        "search\n"};
    for (const char* text : malformed) {
        const auto parsed =
            syscape::detail::network_backend::
                parse_resolver_conf<fake_dns_index_api>(text);
        expect(!parsed &&
                   parsed.error() == syscape::errc::malformed_data,
               "An unusable resolv.conf record is malformed platform data");
    }

    std::string embedded_null("nameserver 192.0.2.1", 20U);
    embedded_null.push_back('\0');
    embedded_null.append("trailing\n");
    const auto truncated_address =
        syscape::detail::network_backend::
            parse_resolver_conf<fake_dns_index_api>(embedded_null);
    expect(!truncated_address &&
               truncated_address.error() == syscape::errc::malformed_data,
           "An embedded null cannot truncate a resolver address");
}

void test_dns_reader_errors() {
    fake_resolver_reader::portable_too_large = false;
    fake_resolver_reader::succeed = false;
    fake_resolver_reader::failure =
        std::error_code(ENOENT, std::generic_category());
    auto loaded = syscape::detail::network_backend::
        load_dns(fake_resolver_reader{});
    expect(!loaded && loaded.error() == syscape::errc::not_found,
           "An absent resolv.conf reports not-found data");

    fake_resolver_reader::failure =
        std::error_code(EACCES, std::generic_category());
    loaded = syscape::detail::network_backend::
        load_dns(fake_resolver_reader{});
    expect(!loaded &&
               loaded.error().value() == EACCES &&
               loaded.error().category() == std::generic_category(),
           "A native reader failure preserves its error code");

    fake_resolver_reader::portable_too_large = true;
    loaded = syscape::detail::network_backend::
        load_dns(fake_resolver_reader{});
    expect(!loaded &&
               loaded.error() ==
                   make_error_code(syscape::errc::value_too_large),
           "An oversized configuration file reports the size limit");
    fake_resolver_reader::succeed = true;
}

void test_dns_boundary_validation() {
    using namespace syscape::detail;
    network_common::ip_address_record address;
    address.family = network_common::address_family::ipv4;
    address.value[0U] = 127U;
    address.value[1U] = 0U;
    address.value[2U] = 0U;
    address.value[3U] = 53U;

    network_common::dns_record valid;
    valid.servers.push_back(network_common::dns_server_record{
        address, std::uint32_t(4U)});
    valid.search_domains.emplace();
    valid.search_domains->push_back("example.net");
    valid.domain_name = "example.net";
    const auto accepted = network_common::validate_dns_record(valid);
    expect(accepted && accepted->servers.size() == 1U,
           "A consistent DNS snapshot passes boundary validation");

    network_common::dns_record unavailable_search = valid;
    unavailable_search.search_domains.reset();
    expect(static_cast<bool>(
               network_common::validate_dns_record(unavailable_search)),
           "An unavailable search list is distinct from an empty list");

    network_common::dns_record zero_binding = valid;
    zero_binding.servers[0U].interface_index = std::uint32_t(0U);
    expect(!network_common::validate_dns_record(zero_binding) &&
               network_common::validate_dns_record(zero_binding).error() ==
                   syscape::errc::malformed_data,
           "A zero interface binding is malformed");

    network_common::dns_record empty_search = valid;
    (*empty_search.search_domains)[0U] = "";
    expect(!network_common::validate_dns_record(empty_search),
           "An empty search-domain entry is malformed");

    network_common::dns_record invalid_encoding = valid;
    (*invalid_encoding.search_domains)[0U] = "bad\xff text";
    expect(network_common::validate_dns_record(invalid_encoding)
                   .error() == syscape::errc::invalid_encoding,
           "A non-UTF-8 search domain reports an encoding failure");

    network_common::dns_record embedded_null = valid;
    (*embedded_null.search_domains)[0U] =
        std::string("good.example\0hidden", 19U);
    expect(network_common::validate_dns_record(embedded_null).error() ==
               syscape::errc::malformed_data,
           "An embedded null search domain is malformed");

    network_common::dns_record embedded_null_domain = valid;
    embedded_null_domain.domain_name =
        std::string("good.example\0hidden", 19U);
    expect(network_common::validate_dns_record(embedded_null_domain).error() ==
               syscape::errc::malformed_data,
           "An embedded null local domain is malformed");

    network_common::dns_record bad_domain = valid;
    bad_domain.domain_name = "";
    expect(!network_common::validate_dns_record(bad_domain),
           "An empty recorded local domain is malformed");

    network_common::dns_record trailing_ipv4_bytes = valid;
    trailing_ipv4_bytes.servers[0U].address.value[4U] = 9U;
    expect(!network_common::validate_dns_record(trailing_ipv4_bytes),
           "IPv4 resolver bytes beyond four are malformed");
}

void test_live_dns() {
    const auto configuration = syscape::network::dns();
    if (!configuration) {
        expect(configuration.error() == syscape::errc::not_found ||
                   configuration.error().category() ==
                       std::generic_category(),
               "Live DNS fails only for absence or a native read error");
        return;
    }

    std::ifstream file("/etc/resolv.conf");
    expect(file.is_open(), "A successful live query implies a readable file");
    std::size_t expected_servers = 0U;
    std::string directive;
    while (file >> directive) {
        std::string value;
        if (directive == "nameserver") { file >> value; }
        file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        if (directive != "nameserver") { continue; }
        const std::size_t zone_start = value.find('%');
        if (zone_start == std::string::npos ||
            ::if_nametoindex(value.substr(zone_start + 1U).c_str()) != 0U) {
            ++expected_servers;
        }
    }
    expect(configuration->servers.size() == expected_servers,
           "Live resolver entries match the documented file's servers");

    for (const syscape::network::dns_server_entry& server :
         configuration->servers) {
        expect(!server.interface_index,
               "The Linux source binds no resolver to an interface");
        if (server.address.family ==
            syscape::network::address_family::ipv4) {
            bool trailing = false;
            for (std::size_t offset = 4U; offset < 16U; ++offset) {
                trailing = trailing ||
                    server.address.value[offset] != 0U;
            }
            expect(!trailing && server.address.scope_id == 0U,
                   "Live IPv4 resolvers carry no trailing bytes or scope");
        }
    }
    expect(configuration->search_domains.has_value(),
           "The Linux file source exposes the search list");
    if (!configuration->search_domains) { return; }
    for (const std::string& domain : *configuration->search_domains) {
        expect(!domain.empty(), "Live search domains are never empty");
    }
    if (configuration->domain_name) {
        expect(!configuration->domain_name->empty() &&
                   configuration->search_domains->size() == 1U &&
                   (*configuration->search_domains)[0U] ==
                       *configuration->domain_name,
               "A recorded local domain matches its single-entry list");
    } else {
        expect(true, "No local domain is a valid live outcome");
    }
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
    test_mtu_failure_is_preserved();
    test_interrupted_mtu_ioctl_is_retried();
    test_boundary_validation();
    test_live_enumeration();
    test_route_boundary_validation();
    test_linux_route_message_conversion();
    test_linux_multipath_and_ipv6_scope();
    test_linux_nexthop_object_route();
    test_live_routes();
    test_dns_parses_documented_file();
    test_dns_domain_directive_is_single_entry_list();
    test_dns_search_and_domain_last_wins();
    test_dns_scoped_nameserver_values();
    test_dns_malformed_records_fail();
    test_dns_reader_errors();
    test_dns_boundary_validation();
    test_live_dns();
    return failures == 0 ? 0 : 1;
}
