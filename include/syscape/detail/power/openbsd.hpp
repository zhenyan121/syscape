#ifndef SYSCAPE_DETAIL_POWER_OPENBSD_HPP
#define SYSCAPE_DETAIL_POWER_OPENBSD_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/sensors.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/power.hpp>
#include <syscape/detail/power/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace power_backend {

inline result<std::vector<power_common::battery_record>> batteries() {
    std::vector<power_common::battery_record> list;
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
        if (xname.find("bat") == std::string::npos &&
            xname.find("acpibat") == std::string::npos) {
            continue;
        }

        power_common::battery_record rec;
        rec.identifier = xname;
        rec.present = true;
        rec.condition = power_common::battery_condition::unknown;

        if (sdev.maxnumt[SENSOR_PERCENT] > 0) {
            mib[3] = SENSOR_PERCENT;
            mib[4] = 0;
            std::size_t s_size = sizeof(s);
            if (::sysctl(mib, 5U, &s, &s_size, nullptr, 0U) == 0) {
                if (s_size != sizeof(s)) {
                    return fail(errc::malformed_data);
                }
                if (!(s.flags & SENSOR_FINVALID) &&
                    !(s.flags & SENSOR_FUNKNOWN)) {
                    if (s.value < 0 || s.value > 100000LL) {
                        return fail(errc::malformed_data);
                    }
                    rec.has_charge_percent = true;
                    rec.charge_percent =
                        static_cast<std::uint32_t>(s.value / 1000);
                }
            } else {
                if (errno != ENOENT && errno != ENXIO && errno != EOPNOTSUPP &&
                    errno != ENOTSUP && errno != ENODEV && errno != EINVAL) {
                    if (errno == EACCES || errno == EPERM) {
                        return fail(errc::permission_denied);
                    }
                    return fail(
                        std::error_code(errno, std::generic_category()));
                }
            }
        }

        list.push_back(std::move(rec));
    }
    return list;
}

inline result<std::vector<power_common::power_source_record>> power_sources() {
    std::vector<power_common::power_source_record> sources;
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
        if (xname.find("acpiac") != std::string::npos || xname == "ac" ||
            xname.rfind("ac ", 0) == 0 || xname.rfind("acadapter", 0) == 0) {
            if (sdev.maxnumt[SENSOR_INDICATOR] > 0) {
                mib[3] = SENSOR_INDICATOR;
                mib[4] = 0;
                std::size_t s_size = sizeof(s);
                if (::sysctl(mib, 5U, &s, &s_size, nullptr, 0U) == 0) {
                    if (s_size != sizeof(s)) {
                        return fail(errc::malformed_data);
                    }
                    if (!(s.flags & SENSOR_FINVALID) &&
                        !(s.flags & SENSOR_FUNKNOWN)) {
                        if (s.value != 0 && s.value != 1) {
                            return fail(errc::malformed_data);
                        }
                        power_common::power_source_record rec;
                        rec.identifier = xname;
                        rec.type = power_common::power_source_type::mains;
                        rec.has_online = true;
                        rec.online = (s.value == 1);
                        sources.push_back(std::move(rec));
                    }
                } else {
                    if (errno != ENOENT && errno != ENXIO &&
                        errno != EOPNOTSUPP && errno != ENOTSUP &&
                        errno != ENODEV && errno != EINVAL) {
                        if (errno == EACCES || errno == EPERM) {
                            return fail(errc::permission_denied);
                        }
                        return fail(
                            std::error_code(errno, std::generic_category()));
                    }
                }
            }
        }
    }
    return sources;
}

inline result<power_common::external_presence> external_power_online() {
    const auto sources = power_sources();
    if (!sources) {
        return fail(sources.error());
    }
    if (sources->empty()) {
        return power_common::external_presence::no_evidence;
    }
    for (const auto& src : *sources) {
        if (src.has_online && src.online) {
            return power_common::external_presence::connected;
        }
    }
    return power_common::external_presence::disconnected;
}

inline result<std::uint64_t> seconds_until_empty() {
    return fail(errc::not_supported);
}

} // namespace power_backend
} // namespace detail
} // namespace syscape

#endif
