#ifndef SYSCAPE_DETAIL_CPU_NETBSD_HPP
#define SYSCAPE_DETAIL_CPU_NETBSD_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/sched.h>
#include <system_error>
#include <utility>
#include <vector>

#include <syscape/cpu.hpp>
#include <syscape/detail/cpu/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace cpu_backend {

inline result<std::string> sysctl_mib_string(const int* mib,
                                             unsigned int mib_len) {
    constexpr int maximum_attempts = 4;
    for (int attempt = 0; attempt < maximum_attempts; ++attempt) {
        std::size_t size = 0U;
        int mib_copy[8];
        if (mib_len > 8U) {
            return fail(errc::not_supported);
        }
        for (unsigned int i = 0; i < mib_len; ++i) {
            mib_copy[i] = mib[i];
        }
        if (::sysctl(mib_copy, mib_len, nullptr, &size, nullptr, 0U) != 0) {
            if (errno == ENOENT) {
                return fail(errc::not_supported);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (size == 0U) {
            return fail(errc::not_found);
        }
        std::string value(size, '\0');
        if (::sysctl(mib_copy, mib_len, &value[0], &size, nullptr, 0U) != 0) {
            if (errno == ENOMEM) {
                continue;
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (size > value.size()) {
            continue;
        }
        value.resize(size);
        while (!value.empty() &&
               (value.back() == '\0' || value.back() == '\n' ||
                value.back() == '\r')) {
            value.pop_back();
        }
        if (!is_valid_utf8(value)) {
            return fail(errc::invalid_encoding);
        }
        return value.empty() ? result<std::string>(fail(errc::not_found))
                             : result<std::string>(std::move(value));
    }
    return fail(errc::temporarily_unavailable);
}

inline result<std::uint32_t> sysctl_mib_uint32(const int* mib,
                                               unsigned int mib_len) {
    int value = 0;
    std::size_t size = sizeof(value);
    int mib_copy[8];
    if (mib_len > 8U) {
        return fail(errc::not_supported);
    }
    for (unsigned int i = 0; i < mib_len; ++i) {
        mib_copy[i] = mib[i];
    }
    if (::sysctl(mib_copy, mib_len, &value, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size != sizeof(value) || value <= 0) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint32_t>(value);
}

inline result<std::vector<std::string>> vendor_identifiers() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> model_names() {
    int mib[] = {CTL_HW, HW_MODEL};
    const auto model = sysctl_mib_string(mib, 2U);
    if (model) {
        return std::vector<std::string> {*model};
    }
    return fail(model.error());
}

inline result<std::uint32_t> online_logical_processor_count() {
#ifdef HW_NCPUONLINE
    int mib_online[] = {CTL_HW, HW_NCPUONLINE};
    auto res_online = sysctl_mib_uint32(mib_online, 2U);
    if (res_online) {
        return res_online;
    }
    if (res_online.error() != errc::not_supported &&
        res_online.error() != errc::not_found) {
        return fail(res_online.error());
    }
#endif
    int mib[] = {CTL_HW, HW_NCPU};
    return sysctl_mib_uint32(mib, 2U);
}

inline result<std::uint32_t> online_physical_core_count() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> online_processor_package_count() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> minimum_frequency_khz() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> maximum_frequency_khz() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::uint32_t>> current_frequencies_khz() {
    return fail(errc::not_supported);
}

inline result<cpu_common::usage_information> cumulative_processor_usage() {
#if defined(KERN_CP_TIME)
    int mib[] = {CTL_KERN, KERN_CP_TIME};
#elif defined(KERN_CPTIME)
    int mib[] = {CTL_KERN, KERN_CPTIME};
#else
    return fail(errc::not_supported);
#endif
    std::uint64_t cp_time[CPUSTATES] = {0};
    std::size_t size = sizeof(cp_time);
    if (::sysctl(mib, 2U, cp_time, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size != sizeof(cp_time)) {
        if (size == sizeof(long) * CPUSTATES) {
            long old_cp_time[CPUSTATES] = {0};
            std::size_t old_size = sizeof(old_cp_time);
            if (::sysctl(mib, 2U, old_cp_time, &old_size, nullptr, 0U) == 0 &&
                old_size == sizeof(old_cp_time)) {
                for (int i = 0; i < CPUSTATES; ++i) {
                    if (old_cp_time[i] < 0) {
                        return fail(errc::malformed_data);
                    }
                    cp_time[i] = static_cast<std::uint64_t>(old_cp_time[i]);
                }
            } else {
                return fail(errc::malformed_data);
            }
        } else {
            return fail(errc::malformed_data);
        }
    }

    const std::uint64_t user = cp_time[CP_USER];
    const std::uint64_t nice = cp_time[CP_NICE];
    const std::uint64_t sys = cp_time[CP_SYS];
    const std::uint64_t intr = cp_time[CP_INTR];
    const std::uint64_t idle = cp_time[CP_IDLE];

    constexpr std::uint64_t max_u64 =
        (std::numeric_limits<std::uint64_t>::max)();
    if (user > max_u64 - nice || sys > max_u64 - intr) {
        return fail(errc::value_too_large);
    }

    cpu_common::usage_information usage;
    usage.user_ticks = user + nice;
    usage.system_ticks = sys + intr;
    usage.idle_ticks = idle;
    return usage;
}

inline result<std::vector<cpu_common::cache_entry>> cache_descriptors() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> instruction_set_features() {
    return fail(errc::not_supported);
}

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#endif
