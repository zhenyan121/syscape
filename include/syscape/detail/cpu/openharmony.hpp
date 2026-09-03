#ifndef SYSCAPE_DETAIL_CPU_OPENHARMONY_HPP
#define SYSCAPE_DETAIL_CPU_OPENHARMONY_HPP

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstdint>
#include <dirent.h>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/detail/cpu/common.hpp>
#include <syscape/detail/openharmony/directory.hpp>
#include <syscape/detail/openharmony/file.hpp>
#include <syscape/detail/openharmony/parameter.hpp>
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
    const auto prop = openharmony::get_parameter("ro.soc.manufacturer");
    if (prop && !prop->empty()) {
        return std::vector<std::string> {*prop};
    }
    if (!prop && prop.error() != errc::not_found &&
        prop.error() != errc::not_supported) {
        return fail(prop.error());
    }

    const auto mfg = openharmony::manufacture();
    if (mfg && !mfg->empty()) {
        return std::vector<std::string> {*mfg};
    }
    if (!mfg && mfg.error() != errc::not_found &&
        mfg.error() != errc::not_supported) {
        return fail(mfg.error());
    }

    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> model_names() {
    const auto prop = openharmony::get_parameter("ro.soc.model");
    if (prop && !prop->empty()) {
        return std::vector<std::string> {*prop};
    }
    if (!prop && prop.error() != errc::not_found &&
        prop.error() != errc::not_supported) {
        return fail(prop.error());
    }

    const auto model = openharmony::product_model();
    if (model && !model->empty()) {
        return std::vector<std::string> {*model};
    }
    if (!model && model.error() != errc::not_found &&
        model.error() != errc::not_supported) {
        return fail(model.error());
    }

    const auto abi = openharmony::get_parameter("const.product.cpu.abi");
    if (abi && !abi->empty()) {
        return std::vector<std::string> {*abi};
    }
    if (!abi && abi.error() != errc::not_found &&
        abi.error() != errc::not_supported) {
        return fail(abi.error());
    }

    return fail(errc::not_supported);
}

inline result<std::vector<std::uint32_t>>
parse_cpu_list(std::string_view input) {
    openharmony::strip_trailing_newlines(input);
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
        if (dash == 0U || dash == item.size() - 1U) {
            return fail(errc::malformed_data);
        }
        if (dash == std::string_view::npos) {
            std::uint32_t val = 0U;
            const auto r =
                std::from_chars(item.data(), item.data() + item.size(), val);
            if (r.ec != std::errc() || r.ptr != item.data() + item.size() ||
                val > maximum_cpu_index) {
                return fail(errc::malformed_data);
            }
            values.insert(val);
        } else {
            const std::string_view start_token = item.substr(0U, dash);
            const std::string_view end_token = item.substr(dash + 1U);
            if (end_token.find('-') != std::string_view::npos) {
                return fail(errc::malformed_data);
            }
            std::uint32_t first = 0U;
            std::uint32_t last = 0U;
            const auto r1 =
                std::from_chars(start_token.data(),
                                start_token.data() + start_token.size(), first);
            const auto r2 = std::from_chars(
                end_token.data(), end_token.data() + end_token.size(), last);
            if (r1.ec != std::errc() ||
                r1.ptr != start_token.data() + start_token.size() ||
                r2.ec != std::errc() ||
                r2.ptr != end_token.data() + end_token.size() || first > last ||
                last > maximum_cpu_index) {
                return fail(errc::malformed_data);
            }
            for (std::uint32_t val = first; val <= last; ++val) {
                values.insert(val);
            }
        }

        if (comma == std::string_view::npos) {
            break;
        }
        offset = comma + 1U;
        if (offset == input.size()) {
            return fail(errc::malformed_data);
        }
    }

    return std::vector<std::uint32_t>(values.begin(), values.end());
}

inline result<std::vector<std::uint32_t>> enumerate_online_cpus() {
    const result<std::string> contents =
        openharmony::read_text_file("/sys/devices/system/cpu/online", 4096U);
    if (!contents) {
        return fail(contents.error());
    }
    return parse_cpu_list(*contents);
}

inline result<std::uint32_t> parse_frequency_text(std::string_view input) {
    std::string_view view = input;
    openharmony::strip_trailing_newlines(view);
    while (!view.empty() && (view.front() == ' ' || view.front() == '\t')) {
        view.remove_prefix(1U);
    }
    if (view.empty()) {
        return fail(errc::malformed_data);
    }
    std::uint64_t freq_khz = 0U;
    const auto r =
        std::from_chars(view.data(), view.data() + view.size(), freq_khz);
    if (r.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (r.ec != std::errc() || r.ptr != view.data() + view.size()) {
        return fail(errc::malformed_data);
    }
    if (freq_khz == 0U) {
        return fail(errc::malformed_data);
    }
    if (freq_khz > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(freq_khz);
}

inline result<std::uint32_t> read_frequency_file(const char* path) {
    const result<std::string> contents = openharmony::read_text_file(path, 64U);
    if (!contents) {
        return fail(contents.error());
    }
    return parse_frequency_text(*contents);
}

inline result<std::uint32_t> read_cpu_frequency(std::uint32_t cpu_index,
                                                const char* filename) {
    char path[128];
    const int written = std::snprintf(
        path, sizeof(path), "/sys/devices/system/cpu/cpu%u/cpufreq/%s",
        static_cast<unsigned int>(cpu_index), filename);
    if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(path)) {
        return fail(errc::malformed_data);
    }
    return read_frequency_file(path);
}

inline result<std::uint32_t> aggregate_frequency_limit(const char* filename,
                                                       bool find_minimum) {
    const result<std::vector<std::uint32_t>> cpus = enumerate_online_cpus();
    if (!cpus) {
        return fail(cpus.error());
    }
    if (cpus->empty()) {
        return fail(errc::not_supported);
    }

    std::uint32_t aggregated = 0U;
    bool found_any = false;
    for (const std::uint32_t cpu_id : *cpus) {
        const result<std::uint32_t> f = read_cpu_frequency(cpu_id, filename);
        if (!f) {
            if (f.error() == errc::not_found ||
                f.error() == errc::not_supported) {
                continue;
            }
            return fail(f.error());
        }
        if (!found_any) {
            aggregated = *f;
            found_any = true;
        } else if (find_minimum) {
            aggregated = (std::min)(aggregated, *f);
        } else {
            aggregated = (std::max)(aggregated, *f);
        }
    }
    if (!found_any) {
        return fail(errc::not_supported);
    }
    return aggregated;
}

inline result<std::vector<std::uint32_t>>
read_cpu_frequencies(const char* filename) {
    const result<std::vector<std::uint32_t>> cpus = enumerate_online_cpus();
    if (!cpus) {
        return fail(cpus.error());
    }
    if (cpus->empty()) {
        return fail(errc::not_supported);
    }

    std::vector<std::uint32_t> freqs;
    freqs.reserve(cpus->size());
    for (const std::uint32_t cpu_id : *cpus) {
        result<std::uint32_t> f = read_cpu_frequency(cpu_id, filename);
        if (!f &&
            (f.error() == errc::not_supported ||
             f.error() == errc::not_found) &&
            std::string_view(filename) == "scaling_cur_freq") {
            f = read_cpu_frequency(cpu_id, "cpuinfo_cur_freq");
        }
        if (!f) {
            return fail(f.error());
        }
        freqs.push_back(*f);
    }
    return freqs;
}

inline result<std::uint32_t> minimum_frequency_khz() {
    result<std::uint32_t> val =
        aggregate_frequency_limit("scaling_min_freq", true);
    if (!val) {
        if (val.error() == errc::not_supported ||
            val.error() == errc::not_found) {
            const auto fallback =
                aggregate_frequency_limit("cpuinfo_min_freq", true);
            if (fallback || fallback.error() != errc::not_supported) {
                return fallback;
            }
        }
        return val;
    }
    return val;
}

inline result<std::uint32_t> maximum_frequency_khz() {
    result<std::uint32_t> val =
        aggregate_frequency_limit("scaling_max_freq", false);
    if (!val) {
        if (val.error() == errc::not_supported ||
            val.error() == errc::not_found) {
            const auto fallback =
                aggregate_frequency_limit("cpuinfo_max_freq", false);
            if (fallback || fallback.error() != errc::not_supported) {
                return fallback;
            }
        }
        return val;
    }
    return val;
}

inline result<std::vector<std::uint32_t>> current_frequencies_khz() {
    return read_cpu_frequencies("scaling_cur_freq");
}

inline result<std::vector<cpu_common::cache_entry>> cache_descriptors() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> instruction_set_features() {
    return fail(errc::not_supported);
}

inline result<cpu_common::usage_information>
parse_proc_stat_usage(std::string_view text) {
    if (text.size() < 4U || text.substr(0, 4) != "cpu ") {
        return fail(errc::malformed_data);
    }

    std::uint64_t fields[10] = {0};
    std::size_t count = 0;
    std::size_t pos = 4;

    while (pos < text.size() && count < 10 && text[pos] != '\n') {
        while (pos < text.size() && text[pos] == ' ') {
            ++pos;
        }
        if (pos >= text.size() || text[pos] == '\n') {
            break;
        }
        const std::size_t start = pos;
        while (pos < text.size() && text[pos] != ' ' && text[pos] != '\n') {
            ++pos;
        }
        const std::string_view token = text.substr(start, pos - start);
        std::uint64_t val = 0;
        const auto r =
            std::from_chars(token.data(), token.data() + token.size(), val);
        if (r.ec == std::errc::result_out_of_range) {
            return fail(errc::value_too_large);
        }
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

inline result<cpu_common::usage_information> cumulative_processor_usage() {
    const result<std::string> stat_data =
        openharmony::read_text_file("/proc/stat", 1024U * 1024U);
    if (!stat_data) {
        return fail(stat_data.error());
    }
    return parse_proc_stat_usage(*stat_data);
}

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#endif
