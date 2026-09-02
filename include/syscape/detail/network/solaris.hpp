#ifndef SYSCAPE_DETAIL_NETWORK_SOLARIS_HPP
#define SYSCAPE_DETAIL_NETWORK_SOLARIS_HPP

#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/detail/network/common.hpp>
#include <syscape/detail/network/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace network_backend {

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
    while (offset < line.size() &&
           (line[offset] == ' ' || line[offset] == '\t')) {
        ++offset;
    }
    if (offset < line.size() && (line[offset] == '#' || line[offset] == ';')) {
        return tokens;
    }
    while (offset < line.size()) {
        while (offset < line.size() &&
               (line[offset] == ' ' || line[offset] == '\t')) {
            ++offset;
        }
        if (offset >= line.size()) {
            break;
        }
        const std::size_t start = offset;
        while (offset < line.size() && line[offset] != ' ' &&
               line[offset] != '\t') {
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
            std::uint64_t scope = 0U;
            for (char ch : zone) {
                if (ch < '0' || ch > '9') {
                    return fail(errc::malformed_data);
                }
                const auto digit = static_cast<std::uint64_t>(ch - '0');
                if (scope > (UINT64_MAX - digit) / 10U) {
                    return fail(errc::value_too_large);
                }
                scope = scope * 10U + digit;
            }
            if (scope > UINT32_MAX) {
                return fail(errc::value_too_large);
            }
            address.scope_id = static_cast<std::uint32_t>(scope);
        }
        return address;
    }

    return fail(errc::malformed_data);
}

inline result<network_common::dns_record> dns() {
    const int fd = ::open("/etc/resolv.conf", O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

    struct fd_guard {
        int d;
        ~fd_guard() {
            if (d >= 0) {
                ::close(d);
            }
        }
    } guard {fd};

    constexpr std::size_t max_file_size = 64U * 1024U;
    std::string contents;
    char buffer[4096];
    for (;;) {
        const ssize_t bytes_read = ::read(fd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            if (contents.size() + static_cast<std::size_t>(bytes_read) >
                max_file_size) {
                return fail(errc::value_too_large);
            }
            contents.append(buffer, static_cast<std::size_t>(bytes_read));
        } else if (bytes_read == 0) {
            break;
        } else if (errno == EINTR) {
            continue;
        } else {
            return fail(std::error_code(errno, std::generic_category()));
        }
    }

    network_common::dns_record record;
    record.search_domains = std::vector<std::string> {};
    std::vector<std::string> search_list;
    std::string domain_value;
    bool has_domain = false;
    bool last_was_search = false;

    std::size_t offset = 0U;
    while (offset < contents.size()) {
        const std::size_t newline_pos = contents.find('\n', offset);
        const std::string_view line =
            (newline_pos == std::string_view::npos)
                ? std::string_view(contents.data() + offset,
                                   contents.size() - offset)
                : std::string_view(contents.data() + offset,
                                   newline_pos - offset);
        offset = (newline_pos == std::string_view::npos) ? contents.size()
                                                         : newline_pos + 1U;

        const auto tokens = tokenize_resolver_line(line);
        if (tokens.empty()) {
            continue;
        }

        const std::string_view directive = tokens[0];
        if (directive == "nameserver") {
            if (tokens.size() != 2U) {
                return fail(errc::malformed_data);
            }
            auto addr = parse_resolver_address(tokens[1]);
            if (!addr) {
                return fail(addr.error());
            }
            network_common::dns_server_record server {};
            server.address = *addr;
            record.servers.push_back(std::move(server));
        } else if (directive == "search") {
            if (tokens.size() < 2U) {
                return fail(errc::malformed_data);
            }
            search_list.clear();
            for (std::size_t i = 1; i < tokens.size(); ++i) {
                search_list.emplace_back(tokens[i]);
            }
            last_was_search = true;
        } else if (directive == "domain") {
            if (tokens.size() != 2U) {
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
    return fail(errc::not_supported);
}

inline result<network_common::statistics_record>
statistics_by_name(std::string_view) {
    return fail(errc::not_supported);
}

inline result<network_common::statistics_record>
statistics_by_index(std::uint32_t) {
    return fail(errc::not_supported);
}

} // namespace network_backend
} // namespace detail
} // namespace syscape

#endif
