#ifndef SYSCAPE_DETAIL_NETWORK_NUTTX_HPP
#define SYSCAPE_DETAIL_NETWORK_NUTTX_HPP

#if defined(__NuttX__)
#include <nuttx/config.h>
#endif

#if defined(__NuttX__) && !defined(CONFIG_NET)
#include <syscape/detail/network/generic.hpp>
#else

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <system_error>
#include <vector>
#include <netinet/in.h>
#include <sys/socket.h>
#if defined(__NuttX__) && defined(CONFIG_NETDB_DNSCLIENT)
#include <nuttx/net/dns.h>
#endif

#include <syscape/detail/network/common.hpp>
#include <syscape/detail/network/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace network_backend {

inline result<std::vector<network_common::route_record>> routes() {
    return fail(errc::not_supported);
}

inline result<network_common::ip_address_record>
nuttx_dns_address(const struct ::sockaddr* address, socklen_t length) {
    if (address == nullptr) {
        return fail(errc::malformed_data);
    }
    network_common::ip_address_record result;
    if (address->sa_family == AF_INET) {
        if (length < static_cast<socklen_t>(sizeof(struct ::sockaddr_in))) {
            return fail(errc::malformed_data);
        }
        const auto* value =
            reinterpret_cast<const struct ::sockaddr_in*>(address);
        result.family = network_common::address_family::ipv4;
        std::memcpy(result.value.data(), &value->sin_addr, 4U);
        return result;
    }
#if defined(AF_INET6)
    if (address->sa_family == AF_INET6) {
        if (length < static_cast<socklen_t>(sizeof(struct ::sockaddr_in6))) {
            return fail(errc::malformed_data);
        }
        const auto* value =
            reinterpret_cast<const struct ::sockaddr_in6*>(address);
        result.family = network_common::address_family::ipv6;
        std::memcpy(result.value.data(), &value->sin6_addr, 16U);
        result.scope_id = value->sin6_scope_id;
        return result;
    }
#endif
    return fail(errc::not_supported);
}

#if defined(__NuttX__) && defined(CONFIG_NETDB_DNSCLIENT)
struct nuttx_dns_context {
    network_common::dns_record record;
    std::error_code error;
};

inline int nuttx_collect_dns(void* opaque, struct ::sockaddr* address,
                             socklen_t length) {
    auto* context = static_cast<nuttx_dns_context*>(opaque);
    const auto converted = nuttx_dns_address(address, length);
    if (!converted) {
        context->error = converted.error();
        return 1;
    }
    network_common::dns_server_record server;
    server.address = *converted;
    context->record.servers.push_back(server);
    return 0;
}
#endif

inline result<network_common::dns_record> dns() {
#if defined(__NuttX__) && defined(CONFIG_NETDB_DNSCLIENT)
    nuttx_dns_context context;
    errno = 0;
    const int outcome = ::dns_foreach_nameserver(&nuttx_collect_dns, &context);
    if (context.error) {
        return fail(context.error);
    }
    if (outcome != 0) {
        const int error = errno;
        return error != 0
                   ? fail(std::error_code(error, std::generic_category()))
                   : fail(errc::io_error);
    }
    // The NuttX resolver API exposes servers but no search-domain list.
    return context.record;
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::vector<network_common::statistics_record>> statistics() {
    return fail(errc::not_supported);
}
inline result<network_common::statistics_record>
statistics_by_name(std::string_view name) {
    (void)name;
    return fail(errc::not_supported);
}
inline result<network_common::statistics_record>
statistics_by_index(std::uint32_t index) {
    (void)index;
    return fail(errc::not_supported);
}

} // namespace network_backend
} // namespace detail
} // namespace syscape

#endif
#endif
