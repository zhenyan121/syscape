#ifndef SYSCAPE_DETAIL_NETWORK_DNS_MACOS_HPP
#define SYSCAPE_DETAIL_NETWORK_DNS_MACOS_HPP

#include <arpa/inet.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <net/if.h>
#include <netinet/in.h>

#include <syscape/detail/network/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace network_backend {

/// The documented dynamic-store state key of the system-wide DNS entity,
/// composed from the documented State domain, the global network entity,
/// and the DNS service key.
constexpr const char* global_dns_state_key = "State:/Network/Global/DNS";

/// Owns one CoreFoundation object reference for the duration of a query.
class cf_object {
public:
    explicit cf_object(::CFTypeRef value) noexcept : value_(value) {}
    cf_object(const cf_object&) = delete;
    cf_object& operator=(const cf_object&) = delete;
    ~cf_object() {
        if (value_ != nullptr) { ::CFRelease(value_); }
    }

    /// Returns the owned reference.
    ::CFTypeRef get() const noexcept { return value_; }

private:
    ::CFTypeRef value_;
};

/// Copies one CoreFoundation string into UTF-8 storage.
///
/// A string that cannot be rendered as UTF-8 reports a conversion failure
/// instead of corrupted text.
inline result<std::string> copy_utf8_string(::CFStringRef value) {
    if (value == nullptr) { return fail(errc::io_error); }
    const ::CFIndex length = ::CFStringGetLength(value);
    if (length == 0) { return std::string(); }
    const ::CFIndex maximum = ::CFStringGetMaximumSizeForEncoding(
        length, ::kCFStringEncodingUTF8);
    if (maximum <= 0) { return fail(errc::io_error); }
    std::string output;
    output.resize(static_cast<std::size_t>(maximum));
    ::CFIndex used = 0;
    const ::CFIndex converted = ::CFStringGetBytes(
        value, ::CFRangeMake(0, length), ::kCFStringEncodingUTF8, 0U,
        false, reinterpret_cast<UInt8*>(&output[0]), maximum, &used);
    if (converted != length || used < 0 || used > maximum) {
        return fail(errc::invalid_encoding);
    }
    output.resize(static_cast<std::size_t>(used));
    return output;
}

/// Converts one documented textual resolver address into binary form.
///
/// SystemConfiguration records ServerAddresses as an array of CFString
/// values. IPv6 zone suffixes accept either a nonzero decimal interface
/// index or an interface name resolved through the documented POSIX API.
/// An unusable address or zone is malformed platform data.
inline result<network_common::ip_address_record> convert_server_address(
    ::CFStringRef value) {
    result<std::string> converted = copy_utf8_string(value);
    if (!converted) { return fail(converted.error()); }
    if (converted->empty() ||
        converted->find('\0') != std::string::npos) {
        return fail(errc::malformed_data);
    }

    std::string literal = *converted;
    std::string zone;
    const std::size_t zone_start = literal.find('%');
    if (zone_start != std::string::npos) {
        zone = literal.substr(zone_start + 1U);
        literal.resize(zone_start);
        if (literal.empty() || zone.empty() ||
            zone.find('%') != std::string::npos) {
            return fail(errc::malformed_data);
        }
    }

    network_common::ip_address_record address;
    ::in_addr ipv4 {};
    if (zone.empty() && ::inet_pton(AF_INET, literal.c_str(), &ipv4) == 1) {
        address.family = network_common::address_family::ipv4;
        const auto* source =
            reinterpret_cast<const unsigned char*>(&ipv4.s_addr);
        for (std::size_t offset = 0U; offset < 4U; ++offset) {
            address.value[offset] = source[offset];
        }
        return address;
    }

    ::in6_addr ipv6 {};
    if (::inet_pton(AF_INET6, literal.c_str(), &ipv6) != 1) {
        return fail(errc::malformed_data);
    }
    address.family = network_common::address_family::ipv6;
    for (std::size_t offset = 0U; offset < 16U; ++offset) {
        address.value[offset] = ipv6.s6_addr[offset];
    }

    if (!zone.empty()) {
        std::uint64_t numeric = 0U;
        bool decimal = true;
        for (char character : zone) {
            if (character < '0' || character > '9') {
                decimal = false;
                break;
            }
            const std::uint64_t digit =
                static_cast<unsigned int>(character - '0');
            const std::uint64_t maximum = static_cast<std::uint64_t>(
                (std::numeric_limits<std::uint32_t>::max)());
            if (numeric > (maximum - digit) / 10U) {
                return fail(errc::malformed_data);
            }
            numeric = numeric * 10U + digit;
        }
        if (decimal) {
            if (numeric == 0U) { return fail(errc::malformed_data); }
            address.scope_id = static_cast<std::uint32_t>(numeric);
        } else {
            errno = 0;
            const unsigned int index = ::if_nametoindex(zone.c_str());
            if (index == 0U) { return fail(errc::malformed_data); }
            address.scope_id = static_cast<std::uint32_t>(index);
        }
    }
    return address;
}

/// Copies one recorded list of resolver-server addresses.
inline result<std::vector<network_common::ip_address_record>>
copy_server_addresses(const ::CFArrayRef value) {
    std::vector<network_common::ip_address_record> addresses;
    const ::CFIndex count = ::CFArrayGetCount(value);
    addresses.reserve(static_cast<std::size_t>(count));
    for (::CFIndex index = 0; index < count; ++index) {
        const ::CFTypeRef element = ::CFArrayGetValueAtIndex(value, index);
        if (element == nullptr ||
            ::CFGetTypeID(element) != ::CFStringGetTypeID()) {
            return fail(errc::malformed_data);
        }
        result<network_common::ip_address_record> address =
            convert_server_address(static_cast<::CFStringRef>(element));
        if (!address) { return fail(address.error()); }
        addresses.push_back(std::move(*address));
    }
    return addresses;
}

/// Copies one recorded list of search-domain strings.
inline result<std::vector<std::string>> copy_search_domains(
    const ::CFArrayRef value) {
    std::vector<std::string> domains;
    const ::CFIndex count = ::CFArrayGetCount(value);
    domains.reserve(static_cast<std::size_t>(count));
    for (::CFIndex index = 0; index < count; ++index) {
        const ::CFTypeRef element = ::CFArrayGetValueAtIndex(value, index);
        if (element == nullptr ||
            ::CFGetTypeID(element) != ::CFStringGetTypeID()) {
            return fail(errc::malformed_data);
        }
        result<std::string> domain =
            copy_utf8_string(static_cast<::CFStringRef>(element));
        if (!domain) { return fail(domain.error()); }
        domains.push_back(std::move(*domain));
    }
    return domains;
}

/// Platform calls used to read the global DNS state.
///
/// The indirection exists so tests can drive loading with synthetic
/// dictionaries instead of the real dynamic store; production callers
/// always use the native implementation.
struct native_sc_store_api {
    /// Returns an owned copy of the State:/Network/Global/DNS dictionary.
    ///
    /// The platform reports failures only as null references, which carry
    /// no standard category, so creation failure maps to io_error and an
    /// absent key maps to not_found because the platform records no
    /// global DNS configuration.
    static result<::CFDictionaryRef> global_dns() {
        const cf_object store(::SCDynamicStoreCreate(
            ::kCFAllocatorDefault, CFSTR("syscape"), nullptr, nullptr));
        if (store.get() == nullptr) { return fail(errc::io_error); }
        ::CFPropertyListRef value = ::SCDynamicStoreCopyValue(
            static_cast<::SCDynamicStoreRef>(store.get()),
            CFSTR("State:/Network/Global/DNS"));
        if (value == nullptr) { return fail(errc::not_found); }
        if (::CFGetTypeID(value) != ::CFDictionaryGetTypeID()) {
            ::CFRelease(value);
            return fail(errc::malformed_data);
        }
        return static_cast<::CFDictionaryRef>(value);
    }
};

/// Collects the platform DNS resolver configuration through the given
/// store API.
///
/// Absent keys record absent fields; empty lists are valid data meaning
/// that the platform records nothing for that field.
template <typename StoreApi>
inline result<network_common::dns_record> collect_dns(StoreApi /*api*/) {
    const result<::CFDictionaryRef> dictionary = StoreApi::global_dns();
    if (!dictionary) { return fail(dictionary.error()); }
    const cf_object owned(*dictionary);
    const ::CFDictionaryRef facts =
        static_cast<::CFDictionaryRef>(owned.get());

    network_common::dns_record collected;
    collected.search_domains.emplace();

    if (const void* servers = ::CFDictionaryGetValue(
            facts, ::kSCPropNetDNSServerAddresses)) {
        if (::CFGetTypeID(servers) != ::CFArrayGetTypeID()) {
            return fail(errc::malformed_data);
        }
        result<std::vector<network_common::ip_address_record>> addresses =
            copy_server_addresses(static_cast<::CFArrayRef>(servers));
        if (!addresses) { return fail(addresses.error()); }
        collected.servers.reserve(addresses->size());
        for (network_common::ip_address_record& address : *addresses) {
            collected.servers.push_back(
                network_common::dns_server_record{std::move(address),
                                                  std::nullopt});
        }
    }

    if (const void* search = ::CFDictionaryGetValue(
            facts, ::kSCPropNetDNSSearchDomains)) {
        if (::CFGetTypeID(search) != ::CFArrayGetTypeID()) {
            return fail(errc::malformed_data);
        }
        result<std::vector<std::string>> domains =
            copy_search_domains(static_cast<::CFArrayRef>(search));
        if (!domains) { return fail(domains.error()); }
        *collected.search_domains = std::move(*domains);
    }

    if (const void* name = ::CFDictionaryGetValue(
            facts, ::kSCPropNetDNSDomainName)) {
        if (::CFGetTypeID(name) != ::CFStringGetTypeID()) {
            return fail(errc::malformed_data);
        }
        result<std::string> domain_name =
            copy_utf8_string(static_cast<::CFStringRef>(name));
        if (!domain_name) { return fail(domain_name.error()); }
        if (domain_name->empty()) { return fail(errc::malformed_data); }
        collected.domain_name = std::move(*domain_name);
    }

    return collected;
}

/// Returns a snapshot of the platform's DNS resolver configuration.
inline result<network_common::dns_record> dns() {
    return collect_dns(native_sc_store_api{});
}

} // namespace network_backend
} // namespace detail
} // namespace syscape

#endif
