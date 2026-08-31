#ifndef SYSCAPE_DETAIL_SENSOR_OPENBSD_HPP
#define SYSCAPE_DETAIL_SENSOR_OPENBSD_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/sensors.h>
#include <system_error>
#include <utility>
#include <vector>

#include <syscape/sensor.hpp>
#include <syscape/detail/sensor/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace sensor_backend {

inline double microkelvin_to_celsius(std::int64_t uK) {
    return (static_cast<double>(uK) - 273150000.0) / 1000000.0;
}

inline result<std::vector<::syscape::sensor::thermal_zone>> thermal_zones() {
    std::vector<::syscape::sensor::thermal_zone> zones;
    int mib[5] = {CTL_HW, HW_SENSORS, 0, 0, 0};
    struct sensordev sdev {};
    struct sensor s {};

    constexpr int maximum_device_index = 65536;
    for (int dev = 0;; ++dev) {
        if (dev >= maximum_device_index) {
            return fail(errc::value_too_large);
        }
        mib[2] = dev;
        std::size_t size = sizeof(sdev);
        if (::sysctl(mib, 3U, &sdev, &size, nullptr, 0U) != 0) {
            if (errno == ENOENT) {
                break;
            }
            if (errno == ENXIO) {
                continue;
            }
            if (errno == EOPNOTSUPP || errno == ENOTSUP || errno == ENODEV ||
                errno == EINVAL) {
                return fail(errc::not_supported);
            }
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (size != sizeof(sdev)) {
            return fail(errc::malformed_data);
        }

        std::string xname(sdev.xname);
        if (xname.find("acpitz") == std::string::npos &&
            xname.find("tz") == std::string::npos) {
            continue;
        }

        if (sdev.maxnumt[SENSOR_TEMP] <= 0) {
            continue;
        }

        for (int num = 0; num < sdev.maxnumt[SENSOR_TEMP]; ++num) {
            mib[3] = SENSOR_TEMP;
            mib[4] = num;
            std::size_t s_size = sizeof(s);
            if (::sysctl(mib, 5U, &s, &s_size, nullptr, 0U) != 0) {
                if (errno == ENOENT || errno == ENXIO) {
                    continue;
                }
                if (errno == EOPNOTSUPP || errno == ENOTSUP ||
                    errno == ENODEV || errno == EINVAL) {
                    return fail(errc::not_supported);
                }
                if (errno == EACCES || errno == EPERM) {
                    return fail(errc::permission_denied);
                }
                return fail(std::error_code(errno, std::generic_category()));
            }
            if (s_size != sizeof(s)) {
                return fail(errc::malformed_data);
            }
            if ((s.flags & SENSOR_FINVALID) || (s.flags & SENSOR_FUNKNOWN)) {
                continue;
            }

            ::syscape::sensor::thermal_zone zone;
            zone.zone_id = xname + "." + std::to_string(num);
            zone.type_name = xname;
            zone.type = sensor_common::classify_thermal_zone(xname);
            zone.current_celsius = microkelvin_to_celsius(s.value);
            zone.enabled = true;
            zones.push_back(std::move(zone));
        }
    }
    return zones;
}

inline result<std::vector<::syscape::sensor::temperature_sensor>>
temperatures() {
    std::vector<::syscape::sensor::temperature_sensor> sensors;
    int mib[5] = {CTL_HW, HW_SENSORS, 0, 0, 0};
    struct sensordev sdev {};
    struct sensor s {};

    constexpr int maximum_device_index = 65536;
    for (int dev = 0;; ++dev) {
        if (dev >= maximum_device_index) {
            return fail(errc::value_too_large);
        }
        mib[2] = dev;
        std::size_t size = sizeof(sdev);
        if (::sysctl(mib, 3U, &sdev, &size, nullptr, 0U) != 0) {
            if (errno == ENOENT) {
                break;
            }
            if (errno == ENXIO) {
                continue;
            }
            if (errno == EOPNOTSUPP || errno == ENOTSUP || errno == ENODEV ||
                errno == EINVAL) {
                return fail(errc::not_supported);
            }
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (size != sizeof(sdev)) {
            return fail(errc::malformed_data);
        }

        std::string xname(sdev.xname);
        if (sdev.maxnumt[SENSOR_TEMP] <= 0) {
            continue;
        }

        for (int num = 0; num < sdev.maxnumt[SENSOR_TEMP]; ++num) {
            mib[3] = SENSOR_TEMP;
            mib[4] = num;
            std::size_t s_size = sizeof(s);
            if (::sysctl(mib, 5U, &s, &s_size, nullptr, 0U) != 0) {
                if (errno == ENOENT || errno == ENXIO) {
                    continue;
                }
                if (errno == EOPNOTSUPP || errno == ENOTSUP ||
                    errno == ENODEV || errno == EINVAL) {
                    return fail(errc::not_supported);
                }
                if (errno == EACCES || errno == EPERM) {
                    return fail(errc::permission_denied);
                }
                return fail(std::error_code(errno, std::generic_category()));
            }
            if (s_size != sizeof(s)) {
                return fail(errc::malformed_data);
            }
            if ((s.flags & SENSOR_FINVALID) || (s.flags & SENSOR_FUNKNOWN)) {
                continue;
            }

            ::syscape::sensor::temperature_sensor item;
            item.device_id = xname + "." + std::to_string(num);
            item.chip_name = xname;
            item.label = s.desc[0] != '\0'
                             ? std::string(s.desc)
                             : (xname + " temp" + std::to_string(num));
            item.type = (xname.rfind("cpu", 0) == 0)
                            ? ::syscape::sensor::temperature_sensor_type::cpu
                            : ::syscape::sensor::temperature_sensor_type::other;
            item.current_celsius = microkelvin_to_celsius(s.value);
            sensors.push_back(std::move(item));
        }
    }
    return sensors;
}

inline result<std::vector<::syscape::sensor::fan_sensor>> fans() {
    std::vector<::syscape::sensor::fan_sensor> fans;
    int mib[5] = {CTL_HW, HW_SENSORS, 0, 0, 0};
    struct sensordev sdev {};
    struct sensor s {};

    constexpr int maximum_device_index = 65536;
    for (int dev = 0;; ++dev) {
        if (dev >= maximum_device_index) {
            return fail(errc::value_too_large);
        }
        mib[2] = dev;
        std::size_t size = sizeof(sdev);
        if (::sysctl(mib, 3U, &sdev, &size, nullptr, 0U) != 0) {
            if (errno == ENOENT) {
                break;
            }
            if (errno == ENXIO) {
                continue;
            }
            if (errno == EOPNOTSUPP || errno == ENOTSUP || errno == ENODEV ||
                errno == EINVAL) {
                return fail(errc::not_supported);
            }
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (size != sizeof(sdev)) {
            return fail(errc::malformed_data);
        }

        std::string xname(sdev.xname);
        if (sdev.maxnumt[SENSOR_FANRPM] <= 0) {
            continue;
        }

        for (int num = 0; num < sdev.maxnumt[SENSOR_FANRPM]; ++num) {
            mib[3] = SENSOR_FANRPM;
            mib[4] = num;
            std::size_t s_size = sizeof(s);
            if (::sysctl(mib, 5U, &s, &s_size, nullptr, 0U) != 0) {
                if (errno == ENOENT || errno == ENXIO) {
                    continue;
                }
                if (errno == EOPNOTSUPP || errno == ENOTSUP ||
                    errno == ENODEV || errno == EINVAL) {
                    return fail(errc::not_supported);
                }
                if (errno == EACCES || errno == EPERM) {
                    return fail(errc::permission_denied);
                }
                return fail(std::error_code(errno, std::generic_category()));
            }
            if (s_size != sizeof(s)) {
                return fail(errc::malformed_data);
            }
            if ((s.flags & SENSOR_FINVALID) || (s.flags & SENSOR_FUNKNOWN)) {
                continue;
            }

            if (s.value < 0) {
                return fail(errc::malformed_data);
            }
            if (static_cast<std::uint64_t>(s.value) >
                (std::numeric_limits<std::uint32_t>::max)()) {
                return fail(errc::value_too_large);
            }

            ::syscape::sensor::fan_sensor fan;
            fan.device_id = xname + "." + std::to_string(num);
            fan.label = s.desc[0] != '\0'
                            ? std::string(s.desc)
                            : (xname + " fan" + std::to_string(num));
            fan.current_rpm = static_cast<std::uint32_t>(s.value);
            fans.push_back(std::move(fan));
        }
    }
    return fans;
}

} // namespace sensor_backend
} // namespace detail
} // namespace syscape

#endif
