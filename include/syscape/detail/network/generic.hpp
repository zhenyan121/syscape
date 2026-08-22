#ifndef SYSCAPE_DETAIL_NETWORK_GENERIC_HPP
#define SYSCAPE_DETAIL_NETWORK_GENERIC_HPP

#include <syscape/detail/network/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace network_backend {

inline result<std::vector<network_common::interface_record>> interfaces() {
    return fail(errc::not_supported);
}

} // namespace network_backend
} // namespace detail
} // namespace syscape

#endif
