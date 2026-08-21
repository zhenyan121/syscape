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
    if (parsed.ec != std::errc() ||
        parsed.ptr != input.data() + input.size()) {
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

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#endif
