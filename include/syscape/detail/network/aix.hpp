#ifndef SYSCAPE_DETAIL_NETWORK_AIX_HPP
#define SYSCAPE_DETAIL_NETWORK_AIX_HPP

#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#if defined(__has_include)
#if __has_include(<libperfstat.h>)
#include <libperfstat.h>
#define SYSCAPE_HAS_AIX_LIBPERFSTAT 1
#endif
#if __has_include(<ifaddrs.h>)
#define SYSCAPE_HAS_AIX_IFADDRS 1
#endif
#endif

#include <syscape/detail/network/common.hpp>
#if defined(SYSCAPE_HAS_AIX_IFADDRS)
#include <syscape/detail/network/posix.hpp>
#endif
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace network_backend {

#if !defined(SYSCAPE_HAS_AIX_IFADDRS)
inline result<std::vector<network_common::interface_record>> interfaces() {
    return fail(errc::not_supported);
}
#endif

inline result<std::vector<network_common::route_record>> routes() {
    return fail(errc::not_supported);
}

inline std::vector<std::string_view>
tokenize_resolver_line(std::string_view line) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.remove_suffix(1U);
    }
    std::vector<std::string_view> tokens;
    std::size_t offset = 0U;
    while (offset < line.size()) {
        while (offset < line.size() &&
               (line[offset] == ' ' || line[offset] == '\t')) {
            ++offset;
        }
        if (offset >= line.size() || line[offset] == '#' ||
            line[offset] == ';') {
            break;
        }
        const std::size_t start = offset;
        while (offset < line.size() && line[offset] != ' ' &&
               line[offset] != '\t' && line[offset] != '#' &&
               line[offset] != ';') {
            ++offset;
        }
        tokens.push_back(line.substr(start, offset - start));
    }
    return tokens;
}

inline result<network_common::ip_address_record>
parse_resolver_address(std::string_view token) {
    std::string_view literal = token;
    std::string_view zone;
    const std::size_t zone_offset = token.find('%');
    if (zone_offset != std::string_view::npos) {
        literal = token.substr(0U, zone_offset);
        zone = token.substr(zone_offset + 1U);
        if (literal.empty() || zone.empty() ||
            zone.find('%') != std::string_view::npos) {
            return fail(errc::malformed_data);
        }
    }
    if (literal.empty() || literal.size() >= INET6_ADDRSTRLEN ||
        literal.find('\0') != std::string_view::npos) {
        return fail(errc::malformed_data);
    }

    char text[INET6_ADDRSTRLEN];
    literal.copy(text, literal.size());
    text[literal.size()] = '\0';

    network_common::ip_address_record address;
    struct ::in_addr ipv4 {};
    if (zone.empty() && ::inet_pton(AF_INET, text, &ipv4) == 1) {
        address.family = network_common::address_family::ipv4;
        const auto* bytes =
            reinterpret_cast<const unsigned char*>(&ipv4.s_addr);
        for (std::size_t index = 0U; index < 4U; ++index) {
            address.value[index] = bytes[index];
        }
        return address;
    }

    struct ::in6_addr ipv6 {};
    if (::inet_pton(AF_INET6, text, &ipv6) == 1) {
        address.family = network_common::address_family::ipv6;
        for (std::size_t index = 0U; index < 16U; ++index) {
            address.value[index] = ipv6.s6_addr[index];
        }
        if (!zone.empty()) {
            std::uint32_t parsed_scope = 0U;
            for (char c : zone) {
                if (c < '0' || c > '9') {
                    return fail(errc::malformed_data);
                }
                const auto digit = static_cast<std::uint32_t>(c - '0');
                if (parsed_scope > (UINT32_MAX - digit) / 10U) {
                    return fail(errc::malformed_data);
                }
                parsed_scope = parsed_scope * 10U + digit;
            }
            address.scope_id = parsed_scope;
        }
        return address;
    }
    return fail(errc::malformed_data);
}

inline result<std::string> read_file_to_string(const char* path) {
    const int fd = ::open(path, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

    std::string content;
    char buffer[1024];
    for (;;) {
        const ssize_t bytes = ::read(fd, buffer, sizeof(buffer));
        if (bytes > 0) {
            content.append(buffer, static_cast<std::size_t>(bytes));
        } else if (bytes == 0) {
            break;
        } else if (errno != EINTR) {
            const int err = errno;
            ::close(fd);
            return fail(std::error_code(err, std::generic_category()));
        }
    }
    ::close(fd);
    return content;
}

inline result<network_common::dns_record> dns() {
    const auto content = read_file_to_string("/etc/resolv.conf");
    if (!content) {
        return fail(content.error());
    }

    network_common::dns_record record;
    record.search_domains.emplace();

    std::vector<std::string> search_list;
    std::string domain_value;
    bool has_domain = false;
    bool last_was_search = false;

    std::string_view remaining(*content);
    while (!remaining.empty()) {
        const std::size_t newline = remaining.find('\n');
        const std::string_view line = newline != std::string_view::npos
                                          ? remaining.substr(0U, newline)
                                          : remaining;
        remaining = newline != std::string_view::npos
                        ? remaining.substr(newline + 1U)
                        : std::string_view();

        const auto tokens = tokenize_resolver_line(line);
        if (tokens.empty()) {
            continue;
        }

        const std::string_view directive = tokens.front();
        if (directive == "nameserver") {
            if (tokens.size() < 2U) {
                return fail(errc::malformed_data);
            }
            const auto addr = parse_resolver_address(tokens[1]);
            if (!addr) {
                return fail(addr.error());
            }
            network_common::dns_server_record server;
            server.address = *addr;
            record.servers.push_back(server);
        } else if (directive == "search") {
            search_list.clear();
            for (std::size_t i = 1U; i < tokens.size(); ++i) {
                search_list.emplace_back(tokens[i]);
            }
            last_was_search = true;
        } else if (directive == "domain") {
            if (tokens.size() < 2U) {
                return fail(errc::malformed_data);
            }
            domain_value.assign(tokens[1]);
            has_domain = true;
            last_was_search = false;
        }
    }

    if (last_was_search) {
        record.search_domains->assign(search_list.begin(), search_list.end());
    } else if (has_domain) {
        record.search_domains->push_back(domain_value);
        record.domain_name = domain_value;
    }
    return record;
}

inline result<std::vector<network_common::statistics_record>> statistics() {
#if defined(SYSCAPE_HAS_AIX_LIBPERFSTAT)
    const int tot = ::perfstat_netinterface(nullptr, nullptr,
                                            sizeof(perfstat_netinterface_t), 0);
    if (tot > 0) {
        std::vector<perfstat_netinterface_t> buffer(
            static_cast<std::size_t>(tot));
        perfstat_id_t first;
        std::strcpy(first.name, FIRST_NETINTERFACE);
        const int ret = ::perfstat_netinterface(
            &first, buffer.data(), sizeof(perfstat_netinterface_t), tot);
        if (ret > 0) {
            std::vector<network_common::statistics_record> records;
            records.reserve(static_cast<std::size_t>(ret));
            for (int i = 0; i < ret; ++i) {
                network_common::statistics_record rec {};
                rec.name = buffer[i].name;
                rec.index = ::if_nametoindex(buffer[i].name);
                if (rec.index == 0U) {
                    rec.index = static_cast<std::uint32_t>(i + 1);
                }
                rec.rx_bytes = static_cast<std::uint64_t>(buffer[i].ibytes);
                rec.tx_bytes = static_cast<std::uint64_t>(buffer[i].obytes);
                rec.rx_packets = static_cast<std::uint64_t>(buffer[i].ipackets);
                rec.tx_packets = static_cast<std::uint64_t>(buffer[i].opackets);
                rec.rx_errors = static_cast<std::uint64_t>(buffer[i].ierrors);
                rec.tx_errors = static_cast<std::uint64_t>(buffer[i].oerrors);
                rec.collisions =
                    static_cast<std::uint64_t>(buffer[i].collisions);
                records.push_back(std::move(rec));
            }
            return records;
        }
    }
#endif
    return fail(errc::not_supported);
}

inline result<network_common::statistics_record>
statistics_by_name(std::string_view name) {
    const auto all = statistics();
    if (!all) {
        return fail(all.error());
    }
    for (const auto& rec : *all) {
        if (rec.name == name) {
            return rec;
        }
    }
    return fail(errc::not_found);
}

inline result<network_common::statistics_record>
statistics_by_index(std::uint32_t index) {
    const auto all = statistics();
    if (!all) {
        return fail(all.error());
    }
    for (const auto& rec : *all) {
        if (rec.index == index) {
            return rec;
        }
    }
    return fail(errc::not_found);
}

} // namespace network_backend
} // namespace detail
} // namespace syscape

#endif
