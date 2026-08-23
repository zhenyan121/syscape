#ifndef SYSCAPE_DETAIL_CPU_LINUX_HPP
#define SYSCAPE_DETAIL_CPU_LINUX_HPP

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iterator>
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

/// Parses one signed decimal sysfs cache attribute.
///
/// The kernel renders plain integers. The recorded unknown marker -1 maps
/// to zero so that callers see the documented not-reported representation;
/// every other negative rendering contradicts the attribute contract and is
/// malformed platform data. Zero remains representable because optional
/// geometry attributes legitimately render zero where a platform records no
/// value.
inline result<std::int64_t> parse_signed_cache_attribute(
    std::string_view input) {
    input = trim_cpu_field(input);
    if (input.empty()) { return fail(errc::malformed_data); }
    std::int64_t value = 0;
    const std::from_chars_result parsed =
        std::from_chars(input.data(), input.data() + input.size(), value);
    if (parsed.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (parsed.ec != std::errc() ||
        parsed.ptr != input.data() + input.size()) {
        return fail(errc::malformed_data);
    }
    if (value == -1) { return 0; }
    if (value < 0) { return fail(errc::malformed_data); }
    return value;
}

/// Parses one unsigned decimal sysfs cache attribute whose zero rendering
/// cannot describe real hardware.
inline result<std::uint32_t> parse_positive_cache_attribute(
    std::string_view input) {
    const result<std::int64_t> value = parse_signed_cache_attribute(input);
    if (!value) { return fail(value.error()); }
    if (*value == 0) { return fail(errc::malformed_data); }
    if (static_cast<std::uint64_t>(*value) >
        static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)())) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(*value);
}

/// Converts an optional cache geometry attribute into the documented
/// not-reported representation.
///
/// Empty text records an attribute that this platform does not expose and
/// yields the documented zero, which no real geometry can equal.
inline result<std::uint32_t> parse_optional_cache_attribute(
    std::string_view input) {
    if (trim_cpu_field(input).empty()) { return 0U; }
    const result<std::int64_t> value = parse_signed_cache_attribute(input);
    if (!value) { return fail(value.error()); }
    if (static_cast<std::uint64_t>(*value) >
        static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)())) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(*value);
}

/// Parses the documented cache size rendering such as "32K" into bytes.
///
/// The kernel renders whole kibibytes; mebibyte and gibibyte suffixes are
/// accepted defensively for forward compatibility. Any other shape,
/// including fractional sizes, is malformed platform data.
inline result<std::uint64_t> parse_cache_size_bytes(std::string_view input) {
    input = trim_cpu_field(input);
    if (input.size() < 2U) { return fail(errc::malformed_data); }
    std::uint64_t multiplier = 0U;
    switch (input.back()) {
    case 'K': multiplier = 1024ULL; break;
    case 'M': multiplier = 1024ULL * 1024ULL; break;
    case 'G': multiplier = 1024ULL * 1024ULL * 1024ULL; break;
    default: return fail(errc::malformed_data);
    }
    const std::string_view digits =
        input.substr(0U, input.size() - 1U);
    std::uint64_t value = 0U;
    const std::from_chars_result parsed =
        std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (parsed.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (parsed.ec != std::errc() ||
        parsed.ptr != digits.data() + digits.size()) {
        return fail(errc::malformed_data);
    }
    constexpr std::uint64_t maximum_size =
        (std::numeric_limits<std::uint64_t>::max)();
    if (value != 0U && multiplier > maximum_size / value) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t bytes = value * multiplier;
    if (bytes == 0U) { return fail(errc::malformed_data); }
    return bytes;
}

/// Maps the documented sysfs cache type rendering onto the portable kind.
inline result<cpu_common::cache_kind> parse_cache_type(
    std::string_view input) {
    input = trim_cpu_field(input);
    if (input == "Data") { return cpu_common::cache_kind::data; }
    if (input == "Instruction") {
        return cpu_common::cache_kind::instruction;
    }
    if (input == "Unified") { return cpu_common::cache_kind::unified; }
    return fail(errc::malformed_data);
}

/// One observed cache instance together with its sharing-processor key.
struct observed_cache_instance {
    cpu_common::cache_entry entry;
    std::vector<std::uint32_t> shared_processors;
};

/// Orders observed instances by level, kind, and sharing set so that the
/// documented output order is deterministic.
inline bool precedes(const observed_cache_instance& left,
                     const observed_cache_instance& right) noexcept {
    if (left.entry.level != right.entry.level) {
        return left.entry.level < right.entry.level;
    }
    if (left.entry.kind != right.entry.kind) {
        return left.entry.kind < right.entry.kind;
    }
    return left.shared_processors < right.shared_processors;
}

/// Returns true when two observations describe the same physical instance.
///
/// A logical processor belongs to exactly one cache instance of a given
/// level and kind, so the sharing set identifies an instance uniquely.
inline bool same_instance(const observed_cache_instance& left,
                          const observed_cache_instance& right) noexcept {
    return left.entry.level == right.entry.level &&
           left.entry.kind == right.entry.kind &&
           left.shared_processors == right.shared_processors;
}

/// Returns true when two observations agree on every recorded attribute.
inline bool same_cache_geometry(const cpu_common::cache_entry& left,
                                const cpu_common::cache_entry& right) noexcept {
    return left.level == right.level && left.kind == right.kind &&
           left.instance_size_bytes == right.instance_size_bytes &&
           left.line_size_bytes == right.line_size_bytes &&
           left.associativity_ways == right.associativity_ways &&
           left.sets_count == right.sets_count &&
           left.shared_logical_processor_count ==
               right.shared_logical_processor_count;
}

/// Returns true when two sorted processor sets overlap.
inline bool processor_sets_overlap(
    const std::vector<std::uint32_t>& left,
    const std::vector<std::uint32_t>& right) noexcept {
    std::size_t left_position = 0U;
    std::size_t right_position = 0U;
    while (left_position < left.size() && right_position < right.size()) {
        if (left[left_position] == right[right_position]) { return true; }
        if (left[left_position] < right[right_position]) {
            ++left_position;
        } else {
            ++right_position;
        }
    }
    return false;
}

/// Reads one required cache attribute.
///
/// A vanished attribute means the sysfs snapshot changed during
/// enumeration and is reported as temporarily unavailable; every other
/// failure preserves its native error code.
inline result<std::string> read_required_cache_attribute(
    const std::string& path) {
    const result<std::string> content =
        linux_platform::read_text_file(path.c_str(), 4096U);
    if (!content &&
        content.error() == std::errc::no_such_file_or_directory) {
        return fail(errc::temporarily_unavailable);
    }
    return content;
}

/// Reads one optional cache attribute.
///
/// A missing recording is valid platform shape and yields empty text that
/// callers translate into the documented not-reported representation.
inline result<std::string> read_optional_cache_attribute(
    const std::string& path) {
    const result<std::string> content =
        linux_platform::read_text_file(path.c_str(), 4096U);
    if (!content &&
        content.error() == std::errc::no_such_file_or_directory) {
        return std::string();
    }
    return content;
}

/// Enumerates every distinct cache instance of one online logical
/// processor.
///
/// The kernel documents contiguous index directories beginning at index0,
/// so a missing type attribute terminates this processor's walk: a missing
/// first cache means the processor exposes no caches at all, and a later
/// gap means the recorded population ended. A processor without any cache
/// directory contributes nothing to the system-wide observation.
inline result<void> observe_processor_caches(
    std::uint32_t cpu, const std::vector<std::uint32_t>& online_processors,
    std::vector<observed_cache_instance>& instances) {
    constexpr unsigned int maximum_cache_index = 1024U;
    const std::string base =
        "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/cache/";
    for (unsigned int index = 0U;; ++index) {
        if (index == maximum_cache_index) {
            return fail(errc::value_too_large);
        }
        const std::string directory = base + "index" + std::to_string(index);

        const result<std::string> type_text =
            read_optional_cache_attribute(directory + "/type");
        if (!type_text) { return fail(type_text.error()); }
        if (type_text->empty()) { return result<void>(); }
        const result<cpu_common::cache_kind> type =
            parse_cache_type(*type_text);
        if (!type) { return fail(type.error()); }

        const result<std::string> level_text =
            read_required_cache_attribute(directory + "/level");
        if (!level_text) { return fail(level_text.error()); }
        const result<std::uint32_t> level =
            parse_positive_cache_attribute(*level_text);
        if (!level) { return fail(level.error()); }

        const result<std::string> size_text =
            read_required_cache_attribute(directory + "/size");
        if (!size_text) { return fail(size_text.error()); }
        const result<std::uint64_t> size =
            parse_cache_size_bytes(*size_text);
        if (!size) { return fail(size.error()); }

        const result<std::string> line_text = read_required_cache_attribute(
            directory + "/coherency_line_size");
        if (!line_text) { return fail(line_text.error()); }
        const result<std::uint32_t> line =
            parse_positive_cache_attribute(*line_text);
        if (!line) { return fail(line.error()); }

        const result<std::string> shared_text =
            read_required_cache_attribute(directory + "/shared_cpu_list");
        if (!shared_text) { return fail(shared_text.error()); }
        const result<std::vector<std::uint32_t>> shared =
            parse_cpu_list(*shared_text);
        if (!shared) { return fail(shared.error()); }
        if (shared->empty()) { return fail(errc::malformed_data); }
        if (!std::binary_search(shared->begin(), shared->end(), cpu)) {
            return fail(errc::temporarily_unavailable);
        }

        std::vector<std::uint32_t> online_shared;
        std::set_intersection(
            shared->begin(), shared->end(), online_processors.begin(),
            online_processors.end(), std::back_inserter(online_shared));
        if (online_shared.empty()) {
            return fail(errc::temporarily_unavailable);
        }

        const result<std::string> ways_text = read_optional_cache_attribute(
            directory + "/ways_of_associativity");
        if (!ways_text) { return fail(ways_text.error()); }
        const result<std::string> sets_text = read_optional_cache_attribute(
            directory + "/number_of_sets");
        if (!sets_text) { return fail(sets_text.error()); }
        const result<std::uint32_t> ways =
            parse_optional_cache_attribute(*ways_text);
        if (!ways) { return fail(ways.error()); }
        const result<std::uint32_t> sets =
            parse_optional_cache_attribute(*sets_text);
        if (!sets) { return fail(sets.error()); }

        observed_cache_instance observed;
        observed.entry.level = *level;
        observed.entry.kind = *type;
        observed.entry.instance_size_bytes = *size;
        observed.entry.line_size_bytes = *line;
        observed.entry.associativity_ways = *ways;
        observed.entry.sets_count = *sets;
        observed.entry.shared_logical_processor_count =
            static_cast<std::uint32_t>(online_shared.size());
        observed.shared_processors = std::move(online_shared);
        instances.push_back(std::move(observed));
    }
}

/// Enumerates one entry per distinct cache instance of the online
/// population, ordered by level, kind, and sharing set.
///
/// The documented testing sysfs ABI lives under
/// /sys/devices/system/cpu/cpuN/cache/indexI/. When no online processor
/// exposes any cache directory, which happens on several virtual machine
/// and embedded configurations, the platform exposes no acceptable source
/// and the query reports not_supported.
inline result<std::vector<cpu_common::cache_entry>> cache_descriptors() {
    const result<std::string> online_text = linux_platform::read_text_file(
        "/sys/devices/system/cpu/online", 1024U * 1024U);
    if (!online_text) { return fail(online_text.error()); }
    const result<std::vector<std::uint32_t>> online =
        parse_cpu_list(*online_text);
    if (!online) { return fail(online.error()); }
    if (online->empty()) { return fail(errc::malformed_data); }

    std::vector<observed_cache_instance> instances;
    for (const std::uint32_t cpu : *online) {
        const result<void> walked =
            observe_processor_caches(cpu, *online, instances);
        if (!walked) { return fail(walked.error()); }
    }

    const result<std::string> online_after_text =
        linux_platform::read_text_file("/sys/devices/system/cpu/online",
                                       1024U * 1024U);
    if (!online_after_text) { return fail(online_after_text.error()); }
    const result<std::vector<std::uint32_t>> online_after =
        parse_cpu_list(*online_after_text);
    if (!online_after) { return fail(online_after.error()); }
    if (*online_after != *online) {
        return fail(errc::temporarily_unavailable);
    }
    if (instances.empty()) {
        return result<std::vector<cpu_common::cache_entry>>(
            fail(errc::not_supported));
    }
    std::sort(instances.begin(), instances.end(), precedes);
    std::vector<observed_cache_instance> unique_instances;
    unique_instances.reserve(instances.size());
    for (const observed_cache_instance& instance : instances) {
        if (!unique_instances.empty() &&
            same_instance(unique_instances.back(), instance)) {
            if (!same_cache_geometry(unique_instances.back().entry,
                                     instance.entry)) {
                return fail(errc::temporarily_unavailable);
            }
            continue;
        }
        for (auto previous = unique_instances.rbegin();
             previous != unique_instances.rend() &&
             previous->entry.level == instance.entry.level &&
             previous->entry.kind == instance.entry.kind;
             ++previous) {
            if (processor_sets_overlap(previous->shared_processors,
                                       instance.shared_processors)) {
                return fail(errc::temporarily_unavailable);
            }
        }
        unique_instances.push_back(instance);
    }

    std::vector<cpu_common::cache_entry> entries;
    entries.reserve(unique_instances.size());
    for (const observed_cache_instance& instance : unique_instances) {
        entries.push_back(instance.entry);
    }
    return entries;
}

/// Returns true when the key names a documented per-architecture
/// instruction-set feature rendering of /proc/cpuinfo.
inline bool is_feature_key(std::string_view key) noexcept {
    return key == "flags" || key == "Features" || key == "features";
}

/// Splits one whitespace-separated feature rendering into unique tokens,
/// preserving first-seen order.
inline void append_feature_tokens(std::vector<std::string>& tokens,
                                  std::string_view value) {
    std::size_t offset = 0U;
    while (offset < value.size()) {
        while (offset < value.size() && cpu_space(value[offset])) {
            ++offset;
        }
        if (offset >= value.size()) { break; }
        const std::size_t start = offset;
        while (offset < value.size() && !cpu_space(value[offset])) {
            ++offset;
        }
        append_unique_label(tokens,
                            value.substr(start, offset - start));
    }
}

/// Extracts the union of instruction-set feature identifiers from
/// /proc/cpuinfo text.
///
/// Every processor block must carry a recognized feature rendering once any
/// block does; partially covered populations contradict the documented
/// per-block format and are malformed platform data. When no block carries
/// a recognized rendering, the architecture exposes no acceptable feature
/// source here and the query reports not_found rather than inventing
/// tokens.
inline result<std::vector<std::string>> parse_cpuinfo_features(
    std::string_view input) {
    std::vector<std::string> tokens;
    std::size_t blocks = 0U;
    std::size_t blocks_with_features = 0U;
    bool pending_block = false;
    bool block_has_features = false;
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
        const std::string_view value =
            trim_cpu_field(line.substr(separator + 1U));

        if (key == "processor") {
            std::uint32_t ignored_index = 0U;
            const std::from_chars_result parsed = std::from_chars(
                value.data(), value.data() + value.size(), ignored_index);
            if (value.empty() || parsed.ec != std::errc() ||
                parsed.ptr != value.data() + value.size()) {
                return fail(errc::malformed_data);
            }
            if (pending_block) {
                if (block_has_features) { ++blocks_with_features; }
            }
            ++blocks;
            pending_block = true;
            block_has_features = false;
            continue;
        }
        if (!is_feature_key(key)) { continue; }
        if (!pending_block) { return fail(errc::malformed_data); }
        if (value.empty()) { return fail(errc::malformed_data); }
        append_feature_tokens(tokens, value);
        block_has_features = true;
    }
    if (pending_block && block_has_features) { ++blocks_with_features; }
    if (blocks == 0U) { return fail(errc::malformed_data); }
    if (blocks_with_features == 0U) {
        return result<std::vector<std::string>>(fail(errc::not_found));
    }
    if (blocks_with_features != blocks) {
        return fail(errc::malformed_data);
    }
    return tokens;
}

inline result<std::vector<std::string>> instruction_set_features() {
    constexpr std::size_t maximum_cpuinfo_size = 16U * 1024U * 1024U;
    const result<std::string> content = linux_platform::read_text_file(
        "/proc/cpuinfo", maximum_cpuinfo_size);
    if (!content) { return fail(content.error()); }
    return parse_cpuinfo_features(*content);
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
