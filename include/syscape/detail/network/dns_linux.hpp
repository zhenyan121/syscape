#ifndef SYSCAPE_DETAIL_NETWORK_DNS_LINUX_HPP
#define SYSCAPE_DETAIL_NETWORK_DNS_LINUX_HPP

#include <arpa/inet.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <net/if.h>

#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/network/common.hpp>
#include <syscape/detail/network/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace network_backend {

/// Splits one resolv.conf line into whitespace-separated tokens.
///
/// The resolv.conf format documents that a line whose semicolon or hash
/// character stands in the first column is a comment, and that keywords
/// and values are separated by white space. Characters after the values
/// are not comment introducers, so they remain part of the tokens and can
/// make an unusable record visible. A trailing carriage return is layout,
/// not content, and is removed before tokenizing.
inline std::vector<std::string_view> tokenize_resolver_line(
    std::string_view line) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.remove_suffix(1U);
    }
    std::vector<std::string_view> tokens;
    std::size_t offset = 0U;
    while (offset < line.size() &&
           (line[offset] == ' ' || line[offset] == '\t')) {
        ++offset;
    }
    if (offset < line.size() &&
        (line[offset] == '#' || line[offset] == ';')) {
        return tokens;
    }
    while (offset < line.size()) {
        while (offset < line.size() &&
               (line[offset] == ' ' || line[offset] == '\t')) {
            ++offset;
        }
        if (offset >= line.size()) { break; }
        const std::size_t start = offset;
        while (offset < line.size() && line[offset] != ' ' &&
               line[offset] != '\t') {
            ++offset;
        }
        tokens.push_back(line.substr(start, offset - start));
    }
    return tokens;
}

/// Converts one documented literal resolver address into its binary form.
///
/// The documented nameserver value is an IP address literal; the
/// platform's own presentation-format converter rejects every other
/// shape.
inline result<network_common::ip_address_record> parse_resolver_address(
    std::string_view token) noexcept {
    network_common::ip_address_record address;
    char text[64];
    if (token.empty() || token.size() >= sizeof(text) ||
        token.find('\0') != std::string_view::npos) {
        return fail(errc::malformed_data);
    }
    token.copy(text, token.size());
    text[token.size()] = '\0';
    ::in_addr ipv4 {};
    if (::inet_pton(AF_INET, text, &ipv4) == 1) {
        address.family = network_common::address_family::ipv4;
        const auto* bytes =
            reinterpret_cast<const unsigned char*>(&ipv4.s_addr);
        for (std::size_t offset = 0U; offset < 4U; ++offset) {
            address.value[offset] = bytes[offset];
        }
        return address;
    }
    ::in6_addr ipv6 {};
    if (::inet_pton(AF_INET6, text, &ipv6) == 1) {
        address.family = network_common::address_family::ipv6;
        for (std::size_t offset = 0U; offset < 16U; ++offset) {
            address.value[offset] = ipv6.s6_addr[offset];
        }
        return address;
    }
    return fail(errc::malformed_data);
}

/// Converts one nameserver value into a resolver record.
///
/// Beyond the documented plain literals this accepts the de-facto
/// address%zone rendering that the platform's own configuration tools
/// emit for link-local resolvers and that the system resolver resolves to
/// a numeric scope: the zone must be an interface name, which is resolved
/// through the documented if_nametoindex interface at query time. A zone
/// naming an interface that no longer exists skips the entry as an
/// expected reconfiguration race instead of failing the snapshot. An IPv4
/// literal never carries a zone, and an empty or nested zone is malformed
/// platform data. The returned optional is empty exactly for the skipped
/// case.
template <typename InterfaceApi = native_interface_api>
inline result<std::optional<network_common::ip_address_record>>
parse_resolver_server(std::string_view token) {
    std::string_view literal = token;
    std::string_view zone;
    bool scoped = false;
    const std::size_t zone_start = token.find('%');
    if (zone_start != std::string_view::npos) {
        scoped = true;
        literal = token.substr(0U, zone_start);
        zone = token.substr(zone_start + 1U);
        if (zone.empty() || zone.find('%') != std::string_view::npos) {
            return fail(errc::malformed_data);
        }
    }

    result<network_common::ip_address_record> address =
        parse_resolver_address(literal);
    if (!address) { return fail(address.error()); }
    if (!scoped) {
        return std::optional<network_common::ip_address_record>(
            std::move(*address));
    }
    if (address->family != network_common::address_family::ipv6) {
        return fail(errc::malformed_data);
    }
    char zone_text[IF_NAMESIZE];
    if (zone.size() >= sizeof(zone_text)) {
        return fail(errc::malformed_data);
    }
    zone.copy(zone_text, zone.size());
    zone_text[zone.size()] = '\0';
    const result<std::uint32_t> index =
        InterfaceApi::index_of(std::string(zone));
    if (!index) {
        // The named interface is gone; the resolver bound to it cannot
        // answer and the platform is mid-reconfiguration at most.
        return std::optional<network_common::ip_address_record> {};
    }
    address->scope_id = *index;
    return std::optional<network_common::ip_address_record>(
        std::move(*address));
}

/// Parses the documented resolv.conf content into a DNS snapshot.
///
/// The nameserver directive takes exactly one address per line. The
/// search and domain directives are documented as mutually exclusive
/// renderings of one list whose last instance overrides all others: the
/// directive kind appearing last in the file determines both the ordered
/// search list and the local domain name, where a domain record is
/// equivalent to a single-entry list. Recognized directives this slice
/// does not represent (options, sortlist) and unrecognized lines are
/// skipped exactly as the documented consumer ignores them. An empty file
/// is valid data meaning no resolver configuration.
template <typename InterfaceApi = native_interface_api>
inline result<network_common::dns_record> parse_resolver_conf(
    std::string_view content) {
    network_common::dns_record parsed;
    parsed.search_domains.emplace();
    std::vector<std::string> search_list;
    bool has_domain = false;
    std::string domain_value;
    bool last_was_search = false;

    std::size_t offset = 0U;
    while (offset < content.size()) {
        const std::size_t end = content.find('\n', offset);
        const std::string_view line(content.data() + offset,
            end == std::string_view::npos ? content.size() - offset
                                          : end - offset);
        offset = end == std::string_view::npos ? content.size() : end + 1U;

        const std::vector<std::string_view> tokens =
            tokenize_resolver_line(line);
        if (tokens.empty()) { continue; }
        const std::string_view directive = tokens.front();

        if (directive == "nameserver") {
            if (tokens.size() != 2U) {
                return fail(errc::malformed_data);
            }
            result<std::optional<network_common::ip_address_record>>
                server = parse_resolver_server<InterfaceApi>(tokens[1U]);
            if (!server) { return fail(server.error()); }
            if (server->has_value()) {
                parsed.servers.push_back(network_common::dns_server_record{
                    **server, std::nullopt});
            }
        } else if (directive == "search") {
            if (tokens.size() < 2U) { return fail(errc::malformed_data); }
            search_list.assign(tokens.begin() + 1, tokens.end());
            last_was_search = true;
        } else if (directive == "domain") {
            if (tokens.size() != 2U) { return fail(errc::malformed_data); }
            domain_value.assign(tokens[1U]);
            has_domain = true;
            last_was_search = false;
        }
        // options, sortlist, and unrecognized directives carry facts this
        // slice does not represent; the documented consumer skips them,
        // so they cannot fail present-day parsing.
    }

    if (last_was_search) {
        parsed.search_domains->assign(search_list.begin(), search_list.end());
    } else if (has_domain) {
        parsed.search_domains->push_back(domain_value);
        parsed.domain_name = domain_value;
    }
    return parsed;
}

/// Platform calls used to read the resolver configuration file.
///
/// The indirection exists so tests can drive loading with synthetic
/// content and failure codes instead of the real filesystem; production
/// callers always use the native implementation.
struct native_resolver_reader {
    /// Reads /etc/resolv.conf, the configuration file the documented
    /// resolver consumes.
    static result<std::string> read() {
        return linux_platform::read_text_file("/etc/resolv.conf");
    }
};

/// Loads and parses the resolver configuration through the given reader.
template <typename Reader,
          typename InterfaceApi = native_interface_api>
inline result<network_common::dns_record> load_dns(Reader /*reader*/) {
    const result<std::string> content = Reader::read();
    if (!content) {
        // An absent configuration file means the platform records no DNS
        // configuration rather than an I/O failure.
        if (content.error() == std::errc::no_such_file_or_directory) {
            return fail(errc::not_found);
        }
        return fail(content.error());
    }
    return parse_resolver_conf<InterfaceApi>(*content);
}

/// Returns a snapshot of the platform's DNS resolver configuration.
inline result<network_common::dns_record> dns() {
    return load_dns(native_resolver_reader{});
}

} // namespace network_backend
} // namespace detail
} // namespace syscape

#endif
