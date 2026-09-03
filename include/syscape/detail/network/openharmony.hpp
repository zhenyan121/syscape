#ifndef SYSCAPE_DETAIL_NETWORK_OPENHARMONY_HPP
#define SYSCAPE_DETAIL_NETWORK_OPENHARMONY_HPP

#include <cstdint>
#include <string_view>
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

inline result<network_common::dns_record> dns() {
    return fail(errc::not_supported);
}

inline result<std::vector<network_common::statistics_record>> statistics() {
    return fail(errc::not_supported);
}

inline result<network_common::statistics_record>
statistics_by_name(std::string_view name) {
    static_cast<void>(name);
    return fail(errc::not_supported);
}

inline result<network_common::statistics_record>
statistics_by_index(std::uint32_t index) {
    static_cast<void>(index);
    return fail(errc::not_supported);
}

} // namespace network_backend
} // namespace detail
} // namespace syscape

#endif
