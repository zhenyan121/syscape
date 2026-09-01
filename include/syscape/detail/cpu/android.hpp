#ifndef SYSCAPE_DETAIL_CPU_ANDROID_HPP
#define SYSCAPE_DETAIL_CPU_ANDROID_HPP

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <dirent.h>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/detail/android/directory.hpp>
#include <syscape/detail/android/file.hpp>
#include <syscape/detail/android/property.hpp>
#include <syscape/detail/cpu/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace cpu_backend {

inline result<std::uint32_t> online_logical_processor_count() {
    errno = 0;
    const long count = ::sysconf(_SC_NPROCESSORS_ONLN);
    if (count <= 0) {
        return errno != 0
                   ? result<std::uint32_t>(
                         fail(std::error_code(errno, std::generic_category())))
                   : result<std::uint32_t>(fail(errc::malformed_data));
    }
    const auto widened = static_cast<unsigned long>(count);
    if (widened > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(widened);
}

inline result<std::uint32_t> online_physical_core_count() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> online_processor_package_count() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> vendor_identifiers() {
    const auto prop = android::get_property("ro.soc.manufacturer");
    if (prop && !prop->empty()) {
        return std::vector<std::string> {*prop};
    }
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> model_names() {
    const auto prop = android::get_property("ro.soc.model");
    if (prop && !prop->empty()) {
        return std::vector<std::string> {*prop};
    }
    const auto board = android::get_property("ro.board.platform");
    if (board && !board->empty()) {
        return std::vector<std::string> {*board};
    }
    return fail(errc::not_supported);
}

inline result<std::vector<std::uint32_t>>
parse_cpu_list(std::string_view input) {
    android::strip_trailing_newlines(input);
    while (!input.empty() && (input.front() == ' ' || input.front() == '\t')) {
        input.remove_prefix(1U);
    }
    while (!input.empty() && (input.back() == ' ' || input.back() == '\t')) {
        input.remove_suffix(1U);
    }
    if (input.empty()) {
        return fail(errc::malformed_data);
    }

    constexpr std::uint32_t maximum_cpu_index = 1024U * 1024U;
    std::set<std::uint32_t> values;
    std::size_t offset = 0U;
    while (offset < input.size()) {
        const std::size_t comma = input.find(',', offset);
        if (comma == offset) {
            return fail(errc::malformed_data);
        }
        std::string_view item = input.substr(
            offset, comma == std::string_view::npos ? input.size() - offset
                                                    : comma - offset);
        while (!item.empty() && (item.front() == ' ' || item.front() == '\t')) {
            item.remove_prefix(1U);
        }
        while (!item.empty() && (item.back() == ' ' || item.back() == '\t')) {
            item.remove_suffix(1U);
        }
        if (item.empty()) {
            return fail(errc::malformed_data);
        }

        const std::size_t dash = item.find('-');
        const std::string_view first_text = item.substr(0U, dash);
        const std::string_view last_text = dash == std::string_view::npos
                                               ? first_text
                                               : item.substr(dash + 1U);
        if (first_text.empty() || last_text.empty() ||
            (dash != std::string_view::npos &&
             item.find('-', dash + 1U) != std::string_view::npos)) {
            return fail(errc::malformed_data);
        }

        std::uint32_t first = 0U;
        std::uint32_t last = 0U;
        const auto parsed_first = std::from_chars(
            first_text.data(), first_text.data() + first_text.size(), first);
        const auto parsed_last = std::from_chars(
            last_text.data(), last_text.data() + last_text.size(), last);
        if (parsed_first.ec != std::errc() ||
            parsed_first.ptr != first_text.data() + first_text.size() ||
            parsed_last.ec != std::errc() ||
            parsed_last.ptr != last_text.data() + last_text.size() ||
            first > last || last > maximum_cpu_index) {
            return fail(errc::malformed_data);
        }
        for (std::uint32_t cpu = first;; ++cpu) {
            values.insert(cpu);
            if (cpu == last) {
                break;
            }
        }

        if (comma == std::string_view::npos) {
            break;
        }
        if (comma + 1U >= input.size()) {
            return fail(errc::malformed_data);
        }
        offset = comma + 1U;
    }
    return std::vector<std::uint32_t>(values.begin(), values.end());
}

inline result<std::vector<std::uint32_t>> enumerate_online_cpus() {
    const auto online_text =
        android::read_text_file("/sys/devices/system/cpu/online");
    if (online_text) {
        return parse_cpu_list(*online_text);
    }
    if (online_text.error() == errc::permission_denied) {
        return fail(errc::permission_denied);
    }
    if (online_text.error() != errc::not_found) {
        return fail(online_text.error());
    }

    // Fallback: iterate /sys/devices/system/cpu directory
    android::directory_handle dir("/sys/devices/system/cpu");
    if (!dir.valid()) {
        if (dir.error() == EACCES || dir.error() == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(dir.error(), std::generic_category()));
    }

    std::vector<std::uint32_t> cpus;
    for (;;) {
        errno = 0;
        struct dirent* entry = ::readdir(dir.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        std::string_view name = entry->d_name;
        if (name.rfind("cpu", 0) != 0 || name.size() <= 3U) {
            continue;
        }
        std::string_view index_str = name.substr(3U);
        std::uint32_t idx = 0U;
        const auto r = std::from_chars(
            index_str.data(), index_str.data() + index_str.size(), idx);
        if (r.ec != std::errc() ||
            r.ptr != index_str.data() + index_str.size()) {
            continue;
        }

        const std::string onl_path =
            std::string("/sys/devices/system/cpu/") + entry->d_name + "/online";
        const auto onl_content = android::read_text_file(onl_path.c_str());
        if (onl_content) {
            std::string_view onl = *onl_content;
            android::strip_trailing_newlines(onl);
            if (onl == "1") {
                cpus.push_back(idx);
            } else if (onl != "0") {
                return fail(errc::malformed_data);
            }
        } else if (onl_content.error() == errc::not_found) {
            // cpu0 often has no online file and is always online
            cpus.push_back(idx);
        } else if (onl_content.error() == errc::permission_denied) {
            return fail(errc::permission_denied);
        } else {
            return fail(onl_content.error());
        }
    }
    std::sort(cpus.begin(), cpus.end());
    return cpus;
}

inline result<std::vector<std::uint32_t>>
collect_frequencies(const char* attr) {
    const auto online_res = enumerate_online_cpus();
    if (!online_res) {
        return fail(online_res.error());
    }

    std::vector<std::uint32_t> freqs;
    for (std::uint32_t cpu : *online_res) {
        const std::string path = "/sys/devices/system/cpu/cpu" +
                                 std::to_string(cpu) + "/cpufreq/" + attr;
        const auto content = android::read_text_file(path.c_str());
        if (!content) {
            if (content.error() == errc::permission_denied) {
                return fail(errc::permission_denied);
            }
            if (content.error() == errc::not_found) {
                continue;
            }
            return fail(content.error());
        }
        std::string_view val = *content;
        android::strip_trailing_newlines(val);
        std::uint32_t freq = 0U;
        const auto res =
            std::from_chars(val.data(), val.data() + val.size(), freq);
        if (res.ec != std::errc() || res.ptr != val.data() + val.size() ||
            freq == 0U) {
            return fail(errc::malformed_data);
        }
        freqs.push_back(freq);
    }

    if (freqs.empty()) {
        return fail(errc::not_supported);
    }
    return freqs;
}

inline result<std::uint32_t> minimum_frequency_khz() {
    const auto freqs = collect_frequencies("cpuinfo_min_freq");
    if (!freqs) {
        return fail(freqs.error());
    }
    return *std::min_element(freqs->begin(), freqs->end());
}

inline result<std::uint32_t> maximum_frequency_khz() {
    const auto freqs = collect_frequencies("cpuinfo_max_freq");
    if (!freqs) {
        return fail(freqs.error());
    }
    return *std::max_element(freqs->begin(), freqs->end());
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
    const auto content = android::read_text_file("/proc/stat");
    if (!content) {
        if (content.error() == errc::permission_denied) {
            return fail(errc::permission_denied);
        }
        return fail(content.error());
    }

    std::string_view text = *content;
    if (text.rfind("cpu ", 0) != 0) {
        return fail(errc::malformed_data);
    }

    text.remove_prefix(4U);
    std::uint64_t fields[10] = {0};
    std::size_t count = 0;
    std::size_t pos = 0;

    while (pos < text.size() && count < 10 && text[pos] != '\n') {
        while (pos < text.size() && text[pos] == ' ')
            ++pos;
        if (pos >= text.size() || text[pos] == '\n')
            break;
        const std::size_t start = pos;
        while (pos < text.size() && text[pos] != ' ' && text[pos] != '\n')
            ++pos;
        const std::string_view token = text.substr(start, pos - start);
        std::uint64_t val = 0;
        const auto r =
            std::from_chars(token.data(), token.data() + token.size(), val);
        if (r.ec != std::errc() || r.ptr != token.data() + token.size()) {
            return fail(errc::malformed_data);
        }
        fields[count++] = val;
    }

    if (count < 4) {
        return fail(errc::malformed_data);
    }

    const std::uint64_t user = fields[0];
    const std::uint64_t nice = fields[1];
    const std::uint64_t system = fields[2];
    const std::uint64_t idle = fields[3];
    const std::uint64_t iowait = count > 4 ? fields[4] : 0ULL;
    const std::uint64_t irq = count > 5 ? fields[5] : 0ULL;
    const std::uint64_t softirq = count > 6 ? fields[6] : 0ULL;

    constexpr std::uint64_t max_u64 =
        (std::numeric_limits<std::uint64_t>::max)();
    if (user > max_u64 - nice || system > max_u64 - irq ||
        (system + irq) > max_u64 - softirq || idle > max_u64 - iowait) {
        return fail(errc::value_too_large);
    }

    cpu_common::usage_information usage {};
    usage.user_ticks = user + nice;
    usage.system_ticks = system + irq + softirq;
    usage.idle_ticks = idle + iowait;
    return usage;
}

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#endif
