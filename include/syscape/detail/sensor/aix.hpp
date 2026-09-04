#ifndef SYSCAPE_DETAIL_SENSOR_AIX_HPP
#define SYSCAPE_DETAIL_SENSOR_AIX_HPP

#include <vector>

#include <syscape/detail/sensor/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace sensor_backend {

inline result<std::vector<::syscape::sensor::temperature_sensor>>
temperatures() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::sensor::fan_sensor>> fans() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::sensor::thermal_zone>> thermal_zones() {
    return fail(errc::not_supported);
}

} // namespace sensor_backend
} // namespace detail
} // namespace syscape

#endif
