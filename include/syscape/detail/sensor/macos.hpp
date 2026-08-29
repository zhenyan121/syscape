#ifndef SYSCAPE_DETAIL_SENSOR_MACOS_HPP
#define SYSCAPE_DETAIL_SENSOR_MACOS_HPP

#include <vector>

#include <syscape/detail/sensor/common.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace sensor_backend {

inline result<std::vector<::syscape::sensor::temperature_sensor>> temperatures() {
    // Stable public unprivileged macOS APIs do not expose per-diode hardware sensor
    // readings (which require private AppleSMC entitlements). Qualitative thermal
    // pressure states do not expose physical temperature measurements.
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::sensor::fan_sensor>> fans() {
    // macOS fan sensors require AppleSMC keys not exposed via stable public APIs.
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::sensor::thermal_zone>> thermal_zones() {
    // Stable public macOS APIs do not expose physical ACPI/hwmon thermal zone tables.
    return fail(errc::not_supported);
}

} // namespace sensor_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_SENSOR_MACOS_HPP
