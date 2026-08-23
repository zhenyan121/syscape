#ifndef SYSCAPE_DETAIL_POWER_GENERIC_HPP
#define SYSCAPE_DETAIL_POWER_GENERIC_HPP

#include <vector>

#include <syscape/detail/power/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace power_backend {

inline result<std::vector<power_common::battery_record>> batteries() {
    return fail(errc::not_supported);
}

inline result<power_common::external_presence> external_power_online() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> seconds_until_empty() {
    return fail(errc::not_supported);
}

} // namespace power_backend
} // namespace detail
} // namespace syscape

#endif
