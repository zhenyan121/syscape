#ifndef SYSCAPE_DETAIL_SENSOR_WINDOWS_HPP
#define SYSCAPE_DETAIL_SENSOR_WINDOWS_HPP

#include <cstdint>
#include <vector>

#include <syscape/detail/sensor/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace sensor_backend {

namespace scomm = ::syscape::detail::sensor_common;

/// Converts a WMI / ACPI temperature in tenths of Kelvin to degrees Celsius.
inline double acpi_tenths_kelvin_to_celsius(std::uint32_t raw_kelvin) noexcept {
    return scomm::tenths_kelvin_to_celsius(static_cast<std::int64_t>(raw_kelvin));
}

inline result<std::vector<::syscape::sensor::temperature_sensor>> temperatures() {
    // No stable Windows backend has been implemented yet.
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::sensor::fan_sensor>> fans() {
    // No stable Windows backend has been implemented yet.
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::sensor::thermal_zone>> thermal_zones() {
    // No stable Windows backend has been implemented yet.
    return fail(errc::not_supported);
}

} // namespace sensor_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_SENSOR_WINDOWS_HPP
