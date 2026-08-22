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

    static void reset() {
        fail_resolution = false;
        next_index = 1U;
    }

    static syscape::result<std::uint32_t> index_of(const std::string&) {
        if (fail_resolution) {
            return syscape::fail(syscape::make_error_code(
                syscape::errc::not_found));
        }
        return next_index++;
    }
};

bool fake_index_api::fail_resolution = false;
std::uint32_t fake_index_api::next_index = 1U;

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

} // namespace

int main() {
    test_link_address_extraction();
    test_empty_link_address();
    test_long_link_address();
    test_inconsistent_link_record_is_malformed();
    test_index_resolution_failure();
    test_live_enumeration();
    return failures == 0 ? 0 : 1;
}
