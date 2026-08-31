#ifndef SYSCAPE_DETAIL_CPU_DRAGONFLY_HPP
#define SYSCAPE_DETAIL_CPU_DRAGONFLY_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/resource.h>

#include <syscape/detail/cpu/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace cpu_backend {

inline result<std::string> sysctl_string(const char* name) {
    constexpr int maximum_attempts = 4;
    for (int attempt = 0; attempt < maximum_attempts; ++attempt) {
        std::size_t size = 0U;
        if (::sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0) {
            if (errno == ENOENT) {
                return fail(errc::not_supported);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (size == 0U) {
            return fail(errc::not_found);
        }
        std::string value(size, '\0');
        if (::sysctlbyname(name, &value[0], &size, nullptr, 0U) != 0) {
            if (errno == ENOMEM) {
                continue;
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (size > value.size()) {
            continue;
        }
        value.resize(size);
        while (!value.empty() && value.back() == '\0') {
            value.pop_back();
        }
        return value.empty() ? result<std::string>(fail(errc::not_found))
                             : result<std::string>(std::move(value));
    }
    return fail(errc::temporarily_unavailable);
}

inline result<std::uint32_t> sysctl_uint32(const char* name) {
    int value = 0;
    std::size_t size = sizeof(value);
    if (::sysctlbyname(name, &value, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size != sizeof(value)) {
        return fail(errc::malformed_data);
    }
    if (value <= 0) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint32_t>(value);
}

inline result<std::vector<std::string>> vendor_identifiers() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> model_names() {
    auto model = sysctl_string("hw.model");
    if (model) {
        return std::vector<std::string> {*model};
    }
    return fail(model.error());
}

inline result<std::uint32_t> online_logical_processor_count() {
    return sysctl_uint32("hw.ncpu");
}

inline result<std::uint32_t> online_physical_core_count() {
    auto cores = sysctl_uint32("kern.smp.cores");
    if (cores) {
        return cores;
    }
    return fail(errc::not_supported);
}

inline result<std::uint32_t> online_processor_package_count() {
    auto packages = sysctl_uint32("kern.smp.packages");
    if (packages) {
        return packages;
    }
    return fail(errc::not_supported);
}

inline result<std::uint32_t> minimum_frequency_khz() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> maximum_frequency_khz() {
    auto clockrate = sysctl_uint32("hw.clockrate");
    if (clockrate) {
        constexpr std::uint32_t max_mhz =
            (std::numeric_limits<std::uint32_t>::max)() / 1000U;
        if (*clockrate > max_mhz) {
            return fail(errc::value_too_large);
        }
        return *clockrate * 1000U;
    }
    return fail(errc::not_supported);
}

inline result<std::vector<std::uint32_t>> current_frequencies_khz() {
    auto count_res = online_logical_processor_count();
    if (!count_res) {
        return fail(count_res.error());
    }
    auto clockrate = sysctl_uint32("hw.clockrate");
    if (clockrate) {
        constexpr std::uint32_t max_mhz =
            (std::numeric_limits<std::uint32_t>::max)() / 1000U;
        if (*clockrate > max_mhz) {
            return fail(errc::value_too_large);
        }
        const std::uint32_t freq_khz = *clockrate * 1000U;
        return std::vector<std::uint32_t>(*count_res, freq_khz);
    }
    return fail(errc::not_supported);
}

inline result<std::vector<cpu_common::cache_entry>> cache_descriptors() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> instruction_set_features() {
    return fail(errc::not_supported);
}

inline result<cpu_common::usage_information> cumulative_processor_usage() {
    long cp_time[5] = {0};
    std::size_t size = sizeof(cp_time);
    if (::sysctlbyname("kern.cp_time", cp_time, &size, nullptr, 0U) != 0 &&
        ::sysctlbyname("kern.cputime", cp_time, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size != sizeof(cp_time)) {
        return fail(errc::malformed_data);
    }
    for (long value : cp_time) {
        if (value < 0) {
            return fail(errc::malformed_data);
        }
    }

    const std::uint64_t user = static_cast<std::uint64_t>(cp_time[0]);
    const std::uint64_t nice = static_cast<std::uint64_t>(cp_time[1]);
    const std::uint64_t system = static_cast<std::uint64_t>(cp_time[2]);
    const std::uint64_t interrupt = static_cast<std::uint64_t>(cp_time[3]);
    constexpr std::uint64_t max_u64 =
        (std::numeric_limits<std::uint64_t>::max)();
    if (user > max_u64 - nice || system > max_u64 - interrupt) {
        return fail(errc::value_too_large);
    }

    cpu_common::usage_information usage;
    usage.user_ticks = user + nice;
    usage.system_ticks = system + interrupt;
    usage.idle_ticks = static_cast<std::uint64_t>(cp_time[4]);
    return usage;
}

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#endif
