#ifndef SYSCAPE_DETAIL_NETWORK_APPLE_MOBILE_HPP
#define SYSCAPE_DETAIL_NETWORK_APPLE_MOBILE_HPP

#include <syscape/detail/network/common.hpp>
#include <syscape/detail/network/posix.hpp>
#include <syscape/detail/network/routes_macos.hpp>
#include <syscape/detail/network/stats_macos.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace network_backend {

inline result<network_common::dns_record> dns() {
    return fail(errc::not_supported);
}

} // namespace network_backend
} // namespace detail
} // namespace syscape

#endif
