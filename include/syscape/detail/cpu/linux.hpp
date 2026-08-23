#ifndef SYSCAPE_DETAIL_CPU_LINUX_HPP
#define SYSCAPE_DETAIL_CPU_LINUX_HPP

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/detail/cpu/common.hpp>
#include <syscape/detail/linux/file.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace cpu_backend {

struct identity_information {
    std::vector<std::string> vendors;
    std::vector<std::string> models;
};

inline bool cpu_space(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

inline std::string_view trim_cpu_field(std::string_view value) noexcept {
    while (!value.empty() && cpu_space(value.front())) { value.remove_prefix(1U); }
    while (!value.empty() && cpu_space(value.back())) { value.remove_suffix(1U); }
    return value;
}

inline void append_unique_label(std::vector<std::string>& labels,
                                std::string_view value) {
    const std::string label(value);
    if (std::find(labels.begin(), labels.end(), label) == labels.end()) {
        labels.push_back(label);
    }
}

inline bool is_vendor_key(std::string_view key) noexcept {
    return key == "vendor_id" || key == "CPU implementer" || key == "vendor";
}

inline bool is_model_key(std::string_view key) noexcept {
    return key == "model name" || key == "Processor" || key == "cpu model";
}

inline result<identity_information> parse_cpuinfo(std::string_view input) {
    identity_information output;
    std::size_t offset = 0U;
    while (offset < input.size()) {
        const std::size_t end = input.find('\n', offset);
        const std::string_view line = input.substr(
            offset, end == std::string_view::npos ? input.size() - offset
                                                   : end - offset);
        offset = end == std::string_view::npos ? input.size() : end + 1U;

        const std::size_t separator = line.find(':');
        if (separator == std::string_view::npos) { continue; }
        const std::string_view key = trim_cpu_field(line.substr(0U, separator));
        const std::string_view value = trim_cpu_field(line.substr(separator + 1U));
        if (!is_vendor_key(key) && !is_model_key(key)) { continue; }
        if (value.empty()) { return fail(errc::malformed_data); }
        if (is_vendor_key(key)) {
            append_unique_label(output.vendors, value);
        } else {
            append_unique_label(output.models, value);
        }
    }
    return output;
}

inline result<identity_information> identity() {
    constexpr std::size_t maximum_cpuinfo_size = 16U * 1024U * 1024U;
    const result<std::string> content = linux_platform::read_text_file(
        "/proc/cpuinfo", maximum_cpuinfo_size);
    if (!content) { return fail(content.error()); }
    return parse_cpuinfo(*content);
}

inline result<std::vector<std::string>> vendor_identifiers() {
    const result<identity_information> value = identity();
    if (!value) { return fail(value.error()); }
    return value->vendors.empty()
               ? result<std::vector<std::string>>(fail(errc::not_found))
               : result<std::vector<std::string>>(value->vendors);
}

inline result<std::vector<std::string>> model_names() {
    const result<identity_information> value = identity();
    if (!value) { return fail(value.error()); }
    return value->models.empty()
               ? result<std::vector<std::string>>(fail(errc::not_found))
               : result<std::vector<std::string>>(value->models);
}

inline result<std::uint32_t> positive_processor_count(long value) {
    if (value < 0) {
        return errno == 0
                   ? result<std::uint32_t>(fail(errc::not_supported))
                   : result<std::uint32_t>(
                         fail(std::error_code(errno, std::generic_category())));
    }
    if (value == 0) { return fail(errc::malformed_data); }
    if (static_cast<unsigned long>(value) >
        (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(value);
}

inline result<std::uint32_t> online_logical_processor_count() {
    errno = 0;
    return positive_processor_count(::sysconf(_SC_NPROCESSORS_ONLN));
}

inline result<std::vector<std::uint32_t>> parse_cpu_list(
    std::string_view input) {
    input = trim_cpu_field(input);
    if (input.empty()) { return fail(errc::malformed_data); }

    constexpr std::uint32_t maximum_cpu_index = 1024U * 1024U;
    std::set<std::uint32_t> values;
    std::size_t offset = 0U;
    while (offset < input.size()) {
        const std::size_t comma = input.find(',', offset);
        const std::string_view item = trim_cpu_field(input.substr(
            offset, comma == std::string_view::npos ? input.size() - offset
                                                     : comma - offset));
        if (item.empty()) { return fail(errc::malformed_data); }

        const std::size_t dash = item.find('-');
        const std::string_view first_text = item.substr(0U, dash);
        const std::string_view last_text =
            dash == std::string_view::npos ? first_text : item.substr(dash + 1U);
        if (first_text.empty() || last_text.empty() ||
            (dash != std::string_view::npos &&
             item.find('-', dash + 1U) != std::string_view::npos)) {
            return fail(errc::malformed_data);
        }

        std::uint32_t first = 0U;
        std::uint32_t last = 0U;
        const std::from_chars_result parsed_first = std::from_chars(
            first_text.data(), first_text.data() + first_text.size(), first);
        const std::from_chars_result parsed_last = std::from_chars(
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
            if (cpu == last) { break; }
        }

        if (comma == std::string_view::npos) { break; }
        offset = comma + 1U;
    }
    return std::vector<std::uint32_t>(values.begin(), values.end());
}

inline result<int> parse_topology_id(std::string_view input) {
    input = trim_cpu_field(input);
    if (input.empty()) { return fail(errc::malformed_data); }
    int value = 0;
    const std::from_chars_result parsed =
        std::from_chars(input.data(), input.data() + input.size(), value);
    if (parsed.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (parsed.ec != std::errc() || parsed.ptr != input.data() + input.size()) {
        return fail(errc::malformed_data);
    }
    if (value == -1) { return fail(errc::not_supported); }
    if (value < 0) { return fail(errc::malformed_data); }
    return value;
}

inline result<int> read_topology_id(std::uint32_t cpu, const char* name) {
    const std::string path = "/sys/devices/system/cpu/cpu" +
                             std::to_string(cpu) + "/topology/" + name;
    const result<std::string> content =
        linux_platform::read_text_file(path.c_str(), 128U);
    if (content) { return parse_topology_id(*content); }
    return content.error() == std::errc::no_such_file_or_directory
               ? result<int>(fail(errc::temporarily_unavailable))
               : result<int>(fail(content.error()));
}

struct topology_information {
    std::uint32_t physical_cores;
    std::uint32_t packages;
};

inline result<topology_information> online_topology() {
    const result<std::string> online_text = linux_platform::read_text_file(
        "/sys/devices/system/cpu/online", 1024U * 1024U);
    if (!online_text) { return fail(online_text.error()); }
    const result<std::vector<std::uint32_t>> online =
        parse_cpu_list(*online_text);
    if (!online) { return fail(online.error()); }
    if (online->empty()) { return fail(errc::malformed_data); }

    std::set<int> packages;
    std::set<std::pair<int, int>> cores;
    for (const std::uint32_t cpu : *online) {
        const result<int> package = read_topology_id(cpu, "physical_package_id");
        if (!package) { return fail(package.error()); }
        const result<int> core = read_topology_id(cpu, "core_id");
        if (!core) { return fail(core.error()); }
        packages.insert(*package);
        cores.emplace(*package, *core);
    }
    if (packages.empty() || cores.empty()) { return fail(errc::malformed_data); }
    if (packages.size() > (std::numeric_limits<std::uint32_t>::max)() ||
        cores.size() > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return topology_information{static_cast<std::uint32_t>(cores.size()),
                                static_cast<std::uint32_t>(packages.size())};
}

inline result<std::uint32_t> online_physical_core_count() {
    const result<topology_information> value = online_topology();
    return value ? result<std::uint32_t>(value->physical_cores)
                 : result<std::uint32_t>(fail(value.error()));
}

inline result<std::uint32_t> online_processor_package_count() {
    const result<topology_information> value = online_topology();
    return value ? result<std::uint32_t>(value->packages)
                 : result<std::uint32_t>(fail(value.error()));
}

/// Parses a whole-text frequency value expressed in kilohertz.
///
/// The sysfs cpufreq attributes render plain decimal integers. Zero cannot
/// describe an operating processor clock, so it is malformed platform data.
inline result<std::uint32_t> parse_khz_text(std::string_view input) {
    input = trim_cpu_field(input);
    if (input.empty()) { return fail(errc::malformed_data); }
    std::uint64_t value = 0U;
    const std::from_chars_result parsed =
        std::from_chars(input.data(), input.data() + input.size(), value);
    if (parsed.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (parsed.ec != std::errc() ||
        parsed.ptr != input.data() + input.size()) {
        return fail(errc::malformed_data);
    }
    if (value == 0U) { return fail(errc::malformed_data); }
    if (value > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(value);
}

/// Parses the /proc/cpuinfo "cpu MHz" rendering into kilohertz.
///
/// The kernel renders a decimal megahertz value such as "800.011". The
/// conversion multiplies by one thousand and rounds half up at the
/// fourth fractional digit; later digits cannot change that rounding.
inline result<std::uint32_t> parse_mhz_text(std::string_view input) {
    input = trim_cpu_field(input);
    if (input.empty()) { return fail(errc::malformed_data); }
    std::size_t offset = 0U;

    constexpr std::uint64_t maximum_megahertz =
        (std::numeric_limits<std::uint32_t>::max)() / 1000U;
    std::uint64_t megahertz = 0U;
    while (offset < input.size() && input[offset] >= '0' &&
           input[offset] <= '9') {
        const std::uint64_t digit =
            static_cast<std::uint64_t>(input[offset] - '0');
        if (megahertz > maximum_megahertz) {
            return fail(errc::value_too_large);
        }
        megahertz = megahertz * 10U + digit;
        ++offset;
    }
    if (offset == 0U) { return fail(errc::malformed_data); }

    std::uint32_t fraction = 0U;
    unsigned int fraction_digits = 0U;
    bool round_up = false;
    if (offset < input.size()) {
        if (input[offset] != '.') { return fail(errc::malformed_data); }
        ++offset;
        while (offset < input.size() && input[offset] >= '0' &&
               input[offset] <= '9') {
            const std::uint32_t digit =
                static_cast<std::uint32_t>(input[offset] - '0');
            if (fraction_digits < 3U) {
                fraction = fraction * 10U + digit;
            } else if (fraction_digits == 3U && digit >= 5U) {
                round_up = true;
            }
            ++fraction_digits;
            ++offset;
        }
        if (fraction_digits == 0U || offset != input.size()) {
            return fail(errc::malformed_data);
        }
    }
    while (fraction_digits < 3U) {
        fraction *= 10U;
        ++fraction_digits;
    }

    std::uint64_t kilohertz = megahertz * 1000U + fraction;
    if (round_up) { ++kilohertz; }
    if (kilohertz == 0U) { return fail(errc::malformed_data); }
    if (kilohertz > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(kilohertz);
}

/// Parses per-processor current frequencies from /proc/cpuinfo text.
///
/// Every "processor" block must carry exactly one "cpu MHz" field; a block
/// without a usable frequency means this source exposes no frequencies and
/// reports not_supported, while a structurally broken record is malformed
/// platform data.
inline result<std::vector<std::uint32_t>> parse_cpuinfo_frequencies(
    std::string_view input) {
    std::vector<std::uint32_t> values;
    std::size_t blocks = 0U;
    bool pending_block = false;
    std::size_t offset = 0U;
    while (offset < input.size()) {
        const std::size_t end = input.find('\n', offset);
        const std::string_view line = input.substr(
            offset, end == std::string_view::npos ? input.size() - offset
                                                   : end - offset);
        offset = end == std::string_view::npos ? input.size() : end + 1U;

        const std::size_t separator = line.find(':');
        if (separator == std::string_view::npos) { continue; }
        const std::string_view key = trim_cpu_field(line.substr(0U, separator));
        const std::string_view value = trim_cpu_field(line.substr(separator + 1U));

        if (key != "processor" && key != "cpu MHz") { continue; }

        if (key == "processor") {
            std::uint32_t ignored_index = 0U;
            const std::from_chars_result parsed = std::from_chars(
                value.data(), value.data() + value.size(), ignored_index);
            if (value.empty() || parsed.ec != std::errc() ||
                parsed.ptr != value.data() + value.size()) {
                return fail(errc::malformed_data);
            }
            if (pending_block) { return fail(errc::malformed_data); }
            ++blocks;
            pending_block = true;
            continue;
        }
        if (!pending_block) { return fail(errc::malformed_data); }
        const result<std::uint32_t> frequency = parse_mhz_text(value);
        if (!frequency) { return fail(frequency.error()); }
        values.push_back(*frequency);
        pending_block = false;
    }
    if (blocks == 0U) { return fail(errc::malformed_data); }
    if (values.empty()) { return fail(errc::not_supported); }
    if (values.size() != blocks) { return fail(errc::malformed_data); }
    return values;
}

inline result<std::vector<std::uint32_t>> cpuinfo_current_frequencies() {
    constexpr std::size_t maximum_cpuinfo_size = 16U * 1024U * 1024U;
    const result<std::string> content = linux_platform::read_text_file(
        "/proc/cpuinfo", maximum_cpuinfo_size);
    if (!content) { return fail(content.error()); }
    return parse_cpuinfo_frequencies(*content);
}

/// Reads the scaling_cur_freq attribute of every listed online processor.
///
/// A missing attribute means the cpufreq subsystem exposes no current
/// frequencies here and yields not_supported so the caller can fall back;
/// any other read failure is a native error.
inline result<std::vector<std::uint32_t>> sysfs_current_frequencies(
    const std::vector<std::uint32_t>& online_cpus) {
    std::vector<std::uint32_t> values;
    values.reserve(online_cpus.size());
    for (const std::uint32_t cpu : online_cpus) {
        const std::string path = "/sys/devices/system/cpu/cpu" +
                                 std::to_string(cpu) +
                                 "/cpufreq/scaling_cur_freq";
        const result<std::string> content =
            linux_platform::read_text_file(path.c_str(), 128U);
        if (!content) {
            return content.error() == std::errc::no_such_file_or_directory
                       ? result<std::vector<std::uint32_t>>(
                             fail(errc::not_supported))
                       : result<std::vector<std::uint32_t>>(
                             fail(content.error()));
        }
        const result<std::uint32_t> value = parse_khz_text(*content);
        if (!value) { return fail(value.error()); }
        values.push_back(*value);
    }
    return values;
}

inline result<std::vector<std::uint32_t>> current_frequencies_khz() {
    const result<std::string> online_text = linux_platform::read_text_file(
        "/sys/devices/system/cpu/online", 1024U * 1024U);
    if (online_text) {
        const result<std::vector<std::uint32_t>> online =
            parse_cpu_list(*online_text);
        if (!online) { return fail(online.error()); }
        const result<std::vector<std::uint32_t>> values =
            sysfs_current_frequencies(*online);
        if (values || values.error() != errc::not_supported) {
            return values;
        }
    } else if (online_text.error() !=
               std::errc::no_such_file_or_directory) {
        return fail(online_text.error());
    }
    return cpuinfo_current_frequencies();
}

/// Reads every available cpufreq bound of the given kind across all online
/// processors.
///
/// Processors without the requested attribute are skipped because some
/// platforms expose cpufreq for only part of their population; other read
/// failures are native errors. An empty collection means no acceptable
/// source exists.
inline result<std::vector<std::uint32_t>> collect_cpufreq_bounds(
    const char* attribute) {
    const result<std::string> online_text = linux_platform::read_text_file(
        "/sys/devices/system/cpu/online", 1024U * 1024U);
    if (!online_text) { return fail(online_text.error()); }
    const result<std::vector<std::uint32_t>> online =
        parse_cpu_list(*online_text);
    if (!online) { return fail(online.error()); }

    std::vector<std::uint32_t> values;
    bool saw_any_attribute = false;
    for (const std::uint32_t cpu : *online) {
        const std::string path = "/sys/devices/system/cpu/cpu" +
                                 std::to_string(cpu) + "/cpufreq/" +
                                 attribute;
        const result<std::string> content =
            linux_platform::read_text_file(path.c_str(), 128U);
        if (!content) {
            if (content.error() ==
                std::errc::no_such_file_or_directory) {
                continue;
            }
            return fail(content.error());
        }
        const result<std::uint32_t> value = parse_khz_text(*content);
        if (!value) { return fail(value.error()); }
        saw_any_attribute = true;
        values.push_back(*value);
    }
    return saw_any_attribute
               ? result<std::vector<std::uint32_t>>(values)
               : result<std::vector<std::uint32_t>>(fail(errc::not_supported));
}

inline result<std::uint32_t> minimum_frequency_khz() {
    const result<std::vector<std::uint32_t>> values =
        collect_cpufreq_bounds("cpuinfo_min_freq");
    if (!values) { return fail(values.error()); }
    return *std::min_element(values->begin(), values->end());
}

inline result<std::uint32_t> maximum_frequency_khz() {
    const result<std::vector<std::uint32_t>> values =
        collect_cpufreq_bounds("cpuinfo_max_freq");
    if (!values) { return fail(values.error()); }
    return *std::max_element(values->begin(), values->end());
}

/// Parses one unsigned /proc/stat counter field exactly.
inline result<std::uint64_t> parse_stat_counter(std::string_view input) {
    std::uint64_t value = 0U;
    const std::from_chars_result parsed =
        std::from_chars(input.data(), input.data() + input.size(), value);
    if (parsed.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (input.empty() || parsed.ec != std::errc() ||
        parsed.ptr != input.data() + input.size()) {
        return fail(errc::malformed_data);
    }
    return value;
}

/// Adds one counter to an accumulator, rejecting representable overflow.
inline result<std::uint64_t> add_counter(std::uint64_t sum,
                                         std::uint64_t value) {
    if (sum >
        (std::numeric_limits<std::uint64_t>::max)() - value) {
        return fail(errc::value_too_large);
    }
    return sum + value;
}

/// Folds the space-separated fields following an aggregate "cpu" line into
/// the portable usage buckets.
///
/// The mapping follows the kernel's documented field order: user and nice
/// (which already include guest time) become user ticks; system, irq, and
/// softirq become system ticks; idle and iowait become idle ticks. Steal
/// time describes execution on behalf of other virtual machines and belongs
/// to no caller-visible bucket, so it is deliberately not counted anywhere.
inline result<cpu_common::usage_information> parse_cpu_usage_fields(
    std::string_view fields) {
    std::vector<std::string_view> items;
    std::size_t offset = 0U;
    while (offset < fields.size()) {
        while (offset < fields.size() && cpu_space(fields[offset])) {
            ++offset;
        }
        if (offset >= fields.size()) { break; }
        const std::size_t start = offset;
        while (offset < fields.size() && !cpu_space(fields[offset])) {
            ++offset;
        }
        items.push_back(fields.substr(start, offset - start));
    }

    if (items.size() < 4U) { return fail(errc::malformed_data); }
    std::vector<std::uint64_t> counters;
    counters.reserve(items.size());
    for (const std::string_view item : items) {
        const result<std::uint64_t> value = parse_stat_counter(item);
        if (!value) { return fail(value.error()); }
        counters.push_back(*value);
    }

    const result<std::uint64_t> user_ticks =
        add_counter(counters[0], counters[1]);
    if (!user_ticks) { return fail(user_ticks.error()); }
    const result<std::uint64_t> system_ticks =
        add_counter(counters[2], items.size() > 5U ? counters[5] : 0U);
    if (!system_ticks) { return fail(system_ticks.error()); }
    const result<std::uint64_t> system_with_softirq = items.size() > 6U
          ? add_counter(*system_ticks, counters[6])
          : system_ticks;
    if (!system_with_softirq) { return fail(system_with_softirq.error()); }
    const result<std::uint64_t> idle_ticks =
        add_counter(counters[3], items.size() > 4U ? counters[4] : 0U);
    if (!idle_ticks) { return fail(idle_ticks.error()); }

    cpu_common::usage_information usage;
    usage.user_ticks = *user_ticks;
    usage.system_ticks = *system_with_softirq;
    usage.idle_ticks = *idle_ticks;
    return usage;
}

/// Parses the aggregate "cpu" line from full /proc/stat text.
///
/// The aggregate line is the first line whose exact prefix is "cpu "
/// followed by counters; per-processor lines carry "cpuN" prefixes and are
/// never mistaken for it. Its absence is reported as not_found rather than
/// being replaced with fabricated totals.
inline result<cpu_common::usage_information> parse_proc_stat_usage(
    std::string_view input) {
    std::size_t offset = 0U;
    while (offset < input.size()) {
        const std::size_t end = input.find('\n', offset);
        const std::string_view line = input.substr(
            offset, end == std::string_view::npos ? input.size() - offset
                                                   : end - offset);
        offset = end == std::string_view::npos ? input.size() : end + 1U;
        if (line.compare(0U, 4U, "cpu ") != 0) { continue; }
        return parse_cpu_usage_fields(line.substr(4U));
    }
    return fail(errc::not_found);
}

inline result<cpu_common::usage_information> cumulative_processor_usage() {
    constexpr std::size_t maximum_stat_size = 16U * 1024U * 1024U;
    const result<std::string> content = linux_platform::read_text_file(
        "/proc/stat", maximum_stat_size);
    if (!content) { return fail(content.error()); }
    return parse_proc_stat_usage(*content);
}

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#endif
