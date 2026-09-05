#ifndef SYSCAPE_DETAIL_CPU_HURD_HPP
#define SYSCAPE_DETAIL_CPU_HURD_HPP

#include <syscape/detail/config.hpp>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/detail/cpu/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace cpu_backend {

inline bool read_line(FILE* fp, std::string& out) {
    out.clear();
    char buf[256];
    while (std::fgets(buf, static_cast<int>(sizeof(buf)), fp) != nullptr) {
        out.append(buf);
        if (!out.empty() && out.back() == '\n') {
            return true;
        }
    }
    return !out.empty();
}

inline result<std::vector<std::string>> vendor_identifiers() {
    FILE* fp = std::fopen("/proc/cpuinfo", "r");
    if (fp == nullptr) {
        const int err = errno;
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err == ENOENT) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(err, std::generic_category()));
    }
    std::string line;
    std::vector<std::string> vendors;
    while (read_line(fp, line)) {
        if (line.rfind("vendor_id", 0) == 0) {
            const auto colon = line.find(':');
            if (colon != std::string::npos) {
                std::size_t val = colon + 1;
                while (val < line.size() &&
                       (line[val] == ' ' || line[val] == '\t')) {
                    ++val;
                }
                std::size_t end = line.size();
                while (end > val &&
                       (line[end - 1] == '\n' || line[end - 1] == '\r' ||
                        line[end - 1] == ' ' || line[end - 1] == '\t')) {
                    --end;
                }
                if (end > val) {
                    std::string vendor = line.substr(val, end - val);
                    if (std::find(vendors.begin(), vendors.end(), vendor) ==
                        vendors.end()) {
                        vendors.push_back(std::move(vendor));
                    }
                }
            }
        }
    }
    const int read_err = std::ferror(fp) ? errno : 0;
    std::fclose(fp);
    if (read_err != 0) {
        if (read_err == EACCES || read_err == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(read_err, std::generic_category()));
    }
    if (!vendors.empty()) {
        return vendors;
    }
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> model_names() {
    FILE* fp = std::fopen("/proc/cpuinfo", "r");
    if (fp == nullptr) {
        const int err = errno;
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err == ENOENT) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(err, std::generic_category()));
    }
    std::string line;
    std::vector<std::string> models;
    while (read_line(fp, line)) {
        if (line.rfind("model name", 0) == 0) {
            const auto colon = line.find(':');
            if (colon != std::string::npos) {
                std::size_t val = colon + 1;
                while (val < line.size() &&
                       (line[val] == ' ' || line[val] == '\t')) {
                    ++val;
                }
                std::size_t end = line.size();
                while (end > val &&
                       (line[end - 1] == '\n' || line[end - 1] == '\r' ||
                        line[end - 1] == ' ' || line[end - 1] == '\t')) {
                    --end;
                }
                if (end > val) {
                    std::string model = line.substr(val, end - val);
                    if (std::find(models.begin(), models.end(), model) ==
                        models.end()) {
                        models.push_back(std::move(model));
                    }
                }
            }
        }
    }
    const int read_err = std::ferror(fp) ? errno : 0;
    std::fclose(fp);
    if (read_err != 0) {
        if (read_err == EACCES || read_err == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(read_err, std::generic_category()));
    }
    if (!models.empty()) {
        return models;
    }
    return fail(errc::not_supported);
}

inline result<std::uint32_t> online_logical_processor_count() {
    errno = 0;
    const long count = ::sysconf(_SC_NPROCESSORS_ONLN);
    if (count < 0) {
        const int saved_errno = errno;
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    if (count == 0) {
        return fail(errc::malformed_data);
    }
    if (static_cast<unsigned long>(count) >
        (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(count);
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

inline result<std::vector<cpu_common::cache_entry>> cache_descriptors() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> instruction_set_features() {
    return fail(errc::not_supported);
}

inline result<cpu_common::usage_information> cumulative_processor_usage() {
    FILE* fp = std::fopen("/proc/stat", "r");
    if (fp == nullptr) {
        const int err = errno;
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err == ENOENT) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(err, std::generic_category()));
    }
    char line[512];
    while (std::fgets(line, static_cast<int>(sizeof(line)), fp) != nullptr) {
        if (std::strncmp(line, "cpu ", 4) == 0) {
            unsigned long long user = 0;
            unsigned long long nice = 0;
            unsigned long long system = 0;
            unsigned long long idle = 0;
            unsigned long long iowait = 0;
            unsigned long long irq = 0;
            unsigned long long softirq = 0;
            unsigned long long steal = 0;
            const int read_count = std::sscanf(
                line + 4, "%llu %llu %llu %llu %llu %llu %llu %llu", &user,
                &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
            std::fclose(fp);
            if (read_count >= 4) {
                if (nice > (std::numeric_limits<std::uint64_t>::max)() - user) {
                    return fail(errc::value_too_large);
                }
                const std::uint64_t user_ticks = user + nice;
                if (irq >
                    (std::numeric_limits<std::uint64_t>::max)() - system) {
                    return fail(errc::value_too_large);
                }
                const std::uint64_t sys_irq = system + irq;
                if (softirq >
                    (std::numeric_limits<std::uint64_t>::max)() - sys_irq) {
                    return fail(errc::value_too_large);
                }
                const std::uint64_t system_ticks = sys_irq + softirq;
                if (iowait >
                    (std::numeric_limits<std::uint64_t>::max)() - idle) {
                    return fail(errc::value_too_large);
                }
                const std::uint64_t idle_ticks = idle + iowait;

                cpu_common::usage_information usage {};
                usage.user_ticks = user_ticks;
                usage.system_ticks = system_ticks;
                usage.idle_ticks = idle_ticks;
                return usage;
            }
            return fail(errc::malformed_data);
        }
    }
    const int read_err = std::ferror(fp) ? errno : 0;
    std::fclose(fp);
    if (read_err != 0) {
        if (read_err == EACCES || read_err == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(read_err, std::generic_category()));
    }
    return fail(errc::not_supported);
}

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#endif
