#ifndef SYSCAPE_DETAIL_NETWORK_FREEBSD_HPP
#define SYSCAPE_DETAIL_NETWORK_FREEBSD_HPP

#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/detail/network/common.hpp>
#include <syscape/detail/network/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace network_backend {

inline result<std::vector<network_common::interface_record>> interfaces() {
    return network_posix::interfaces();
}

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
    struct ::in_addr ipv4{};
    if (zone.empty() && ::inet_pton(AF_INET, text, &ipv4) == 1) {
        address.family = network_common::address_family::ipv4;
        const auto* bytes =
            reinterpret_cast<const unsigned char*>(&ipv4.s_addr);
        for (std::size_t index = 0U; index < 4U; ++index) {
            address.value[index] = bytes[index];
        }
        return address;
    }

    struct ::in6_addr ipv6{};
    if (::inet_pton(AF_INET6, text, &ipv6) != 1) {
        return fail(errc::malformed_data);
    }
    address.family = network_common::address_family::ipv6;
    for (std::size_t index = 0U; index < 16U; ++index) {
        address.value[index] = ipv6.s6_addr[index];
    }

    if (!zone.empty()) {
        if (zone.size() >= IF_NAMESIZE ||
            zone.find('\0') != std::string_view::npos) {
            return fail(errc::malformed_data);
        }
        char interface_name[IF_NAMESIZE];
        zone.copy(interface_name, zone.size());
        interface_name[zone.size()] = '\0';
        errno = 0;
        const unsigned int index = ::if_nametoindex(interface_name);
        if (index == 0U) {
            return errno != 0
                       ? result<network_common::ip_address_record>(fail(
                             std::error_code(errno, std::generic_category())))
                       : result<network_common::ip_address_record>(
                             fail(errc::temporarily_unavailable));
        }
        address.scope_id = static_cast<std::uint32_t>(index);
    }
    return address;
}

class file_descriptor {
    public:
    explicit file_descriptor(int value) noexcept : value_(value) {}
    file_descriptor(const file_descriptor&) = delete;
    file_descriptor& operator=(const file_descriptor&) = delete;
    ~file_descriptor() {
        if (value_ >= 0) {
            static_cast<void>(::close(value_));
        }
    }

    int get() const noexcept {
        return value_;
    }

    private:
    int value_;
};

inline result<std::string> read_resolv_conf() {
    const int fd = ::open("/etc/resolv.conf", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    const file_descriptor owned_fd(fd);
    std::string content;
    char buffer[4096];
    for (;;) {
        const ssize_t bytes = ::read(owned_fd.get(), buffer, sizeof(buffer));
        if (bytes < 0) {
            if (errno == EINTR) {
                continue;
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (bytes == 0) {
            break;
        }
        content.append(buffer, static_cast<std::size_t>(bytes));
    }
    return content;
}

class ifaddrs_list {
    public:
    explicit ifaddrs_list(struct ::ifaddrs* value) noexcept : value_(value) {}
    ifaddrs_list(const ifaddrs_list&) = delete;
    ifaddrs_list& operator=(const ifaddrs_list&) = delete;
    ~ifaddrs_list() {
        if (value_ != nullptr) {
            ::freeifaddrs(value_);
        }
    }

    private:
    struct ::ifaddrs* value_;
};

inline result<network_common::dns_record> dns() {
    const auto content_res = read_resolv_conf();
    if (!content_res) {
        return fail(content_res.error());
    }

    network_common::dns_record parsed;
    parsed.search_domains = std::vector<std::string>{};
    std::vector<std::string> search_list;
    std::string domain_value;
    bool has_domain = false;
    bool last_was_search = false;

    std::size_t line_start = 0U;
    const std::string& content = *content_res;
    while (line_start < content.size()) {
        std::size_t line_end = content.find('\n', line_start);
        std::string_view line =
            (line_end == std::string::npos)
                ? std::string_view(content.data() + line_start,
                                   content.size() - line_start)
                : std::string_view(content.data() + line_start,
                                   line_end - line_start);
        line_start =
            (line_end == std::string::npos) ? content.size() : line_end + 1U;

        const auto tokens = tokenize_resolver_line(line);
        if (tokens.empty()) {
            continue;
        }
        const std::string_view directive = tokens[0];
        if (directive == "nameserver") {
            if (tokens.size() != 2U) {
                return fail(errc::malformed_data);
            }
            const auto ip = parse_resolver_address(tokens[1]);
            if (!ip) {
                return fail(ip.error());
            }
            network_common::dns_server_record server;
            server.address = *ip;
            parsed.servers.push_back(std::move(server));
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
        parsed.search_domains->assign(search_list.begin(), search_list.end());
    } else if (has_domain) {
        parsed.search_domains->push_back(domain_value);
        parsed.domain_name = domain_value;
    }
    return parsed;
}

inline result<std::vector<network_common::statistics_record>> statistics() {
    struct ::ifaddrs* head = nullptr;
    if (::getifaddrs(&head) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    const ifaddrs_list owned_head(head);
    std::vector<network_common::statistics_record> results;
    for (const struct ::ifaddrs* cursor = head; cursor != nullptr;
         cursor = cursor->ifa_next) {
        if (cursor->ifa_name == nullptr || cursor->ifa_addr == nullptr) {
            continue;
        }
        if (cursor->ifa_addr->sa_family == AF_LINK &&
            cursor->ifa_data != nullptr) {
            const struct ::if_data* data =
                reinterpret_cast<const struct ::if_data*>(cursor->ifa_data);
            network_common::statistics_record record;
            record.name = cursor->ifa_name;
            errno = 0;
            const unsigned int index = ::if_nametoindex(cursor->ifa_name);
            if (index == 0U) {
                return errno != 0 ? result<std::vector<
                                        network_common::statistics_record>>(
                                        fail(std::error_code(
                                            errno, std::generic_category())))
                                  : result<std::vector<
                                        network_common::statistics_record>>(
                                        fail(errc::temporarily_unavailable));
            }
            record.index = static_cast<std::uint32_t>(index);
            record.rx_bytes = data->ifi_ibytes;
            record.tx_bytes = data->ifi_obytes;
            record.rx_packets = data->ifi_ipackets;
            record.tx_packets = data->ifi_opackets;
            record.rx_errors = data->ifi_ierrors;
            record.tx_errors = data->ifi_oerrors;
            record.rx_drops = data->ifi_iqdrops;
            record.tx_drops = data->ifi_oqdrops;
            record.multicast_rx_packets = data->ifi_imcasts;
            record.multicast_tx_packets = data->ifi_omcasts;
            record.collisions = data->ifi_collisions;
            results.push_back(std::move(record));
        }
    }
    return results;
}

inline result<network_common::statistics_record>
statistics_by_name(std::string_view name) {
    const auto all = statistics();
    if (!all) {
        return fail(all.error());
    }
    for (const auto& entry : *all) {
        if (entry.name == name) {
            return entry;
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
    for (const auto& entry : *all) {
        if (entry.index == index) {
            return entry;
        }
    }
    return fail(errc::not_found);
}

} // namespace network_backend
} // namespace detail
} // namespace syscape

#endif
