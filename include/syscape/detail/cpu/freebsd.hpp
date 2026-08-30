#ifndef SYSCAPE_DETAIL_CPU_FREEBSD_HPP
#define SYSCAPE_DETAIL_CPU_FREEBSD_HPP

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
        return std::vector<std::string>{*model};
    }
    return fail(model.error());
}

inline result<std::uint32_t> online_logical_processor_count() {
    return sysctl_uint32("hw.ncpu");
}

inline result<std::uint32_t> online_physical_core_count() {
    return sysctl_uint32("kern.smp.cores");
}

inline result<std::uint32_t> online_processor_package_count() {
    return sysctl_uint32("kern.smp.packages");
}

struct freq_bounds {
    std::uint32_t min_khz = 0U;
    std::uint32_t max_khz = 0U;
};

inline result<freq_bounds> query_freq_levels() {
    auto levels_str = sysctl_string("dev.cpu.0.freq_levels");
    if (!levels_str) {
        return fail(levels_str.error());
    }
    if (levels_str->empty()) {
        return fail(errc::not_found);
    }

    std::uint32_t min_freq = (std::numeric_limits<std::uint32_t>::max)();
    std::uint32_t max_freq = 0U;
    bool found = false;

    std::size_t offset = 0U;
    const std::string& str = *levels_str;
    while (offset < str.size()) {
        while (offset < str.size() &&
               (str[offset] == ' ' || str[offset] == '\t')) {
            ++offset;
        }
        if (offset >= str.size()) {
            break;
        }
        std::uint64_t val = 0U;
        bool has_digits = false;
        constexpr std::uint64_t max_mhz =
            (std::numeric_limits<std::uint32_t>::max)() / 1000U;
        while (offset < str.size() && str[offset] >= '0' &&
               str[offset] <= '9') {
            has_digits = true;
            const unsigned char digit =
                static_cast<unsigned char>(str[offset] - '0');
            if (val > (max_mhz - digit) / 10U) {
                return fail(errc::value_too_large);
            }
            val = val * 10U + digit;
            ++offset;
        }
        if (!has_digits || offset >= str.size() || str[offset] != '/') {
            return fail(errc::malformed_data);
        }
        if (val > 0U) {
            std::uint32_t freq_khz = static_cast<std::uint32_t>(val * 1000U);
            if (freq_khz < min_freq) {
                min_freq = freq_khz;
            }
            if (freq_khz > max_freq) {
                max_freq = freq_khz;
            }
            found = true;
        }
        while (offset < str.size() && str[offset] != ' ' &&
               str[offset] != '\t') {
            ++offset;
        }
    }

    if (!found || max_freq == 0U) {
        return fail(errc::malformed_data);
    }

    freq_bounds bounds;
    bounds.min_khz = min_freq;
    bounds.max_khz = max_freq;
    return bounds;
}

inline result<std::uint32_t> minimum_frequency_khz() {
    const auto bounds = query_freq_levels();
    if (!bounds) {
        return fail(bounds.error());
    }
    return bounds->min_khz;
}

inline result<std::uint32_t> maximum_frequency_khz() {
    const auto bounds = query_freq_levels();
    if (!bounds) {
        return fail(bounds.error());
    }
    return bounds->max_khz;
}

inline result<std::vector<std::uint32_t>> current_frequencies_khz() {
    auto count_res = online_logical_processor_count();
    if (!count_res) {
        return fail(count_res.error());
    }
    constexpr std::uint32_t max_khz_bound =
        (std::numeric_limits<std::uint32_t>::max)() / 1000U;
    std::vector<std::uint32_t> freqs;
    freqs.reserve(*count_res);
    for (std::uint32_t i = 0; i < *count_res; ++i) {
        std::string name = "dev.cpu." + std::to_string(i) + ".freq";
        auto freq = sysctl_uint32(name.c_str());
        if (freq) {
            if (*freq > max_khz_bound) {
                return fail(errc::value_too_large);
            }
            freqs.push_back(*freq * 1000U);
        } else if (freq.error() == errc::not_supported) {
            auto def_freq = sysctl_uint32("dev.cpu.0.freq");
            if (def_freq) {
                if (*def_freq > max_khz_bound) {
                    return fail(errc::value_too_large);
                }
                freqs.push_back(*def_freq * 1000U);
            } else {
                return fail(def_freq.error());
            }
        } else {
            return fail(freq.error());
        }
    }
    return freqs;
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
    if (::sysctlbyname("kern.cp_time", cp_time, &size, nullptr, 0U) != 0) {
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
