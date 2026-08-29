#ifndef SYSCAPE_DETAIL_NUMA_LINUX_HPP
#define SYSCAPE_DETAIL_NUMA_LINUX_HPP

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#if defined(__has_include)
#if __has_include(<sys/syscall.h>)
#include <sys/syscall.h>
#endif
#endif

#include <sched.h>

#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/numa/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace numa_backend {

inline bool numa_space(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

inline std::string_view trim_numa_field(std::string_view value) noexcept {
    while (!value.empty() && numa_space(value.front())) { value.remove_prefix(1U); }
    while (!value.empty() && numa_space(value.back())) { value.remove_suffix(1U); }
    return value;
}

inline result<std::vector<std::uint32_t>> parse_range_list(
    std::string_view input) {
    input = trim_numa_field(input);
    if (input.empty() || input.front() == ',' || input.back() == ',') {
        return fail(errc::malformed_data);
    }

    constexpr std::uint32_t maximum_index = 1024U * 1024U;
    std::set<std::uint32_t> values;
    std::size_t offset = 0U;
    while (offset < input.size()) {
        const std::size_t comma = input.find(',', offset);
        const std::string_view item = trim_numa_field(input.substr(
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
            first > last || last > maximum_index) {
            return fail(errc::malformed_data);
        }
        for (std::uint32_t i = first;; ++i) {
            values.insert(i);
            if (i == last) { break; }
        }

        if (comma == std::string_view::npos) { break; }
        offset = comma + 1U;
    }
    if (values.empty()) {
        return fail(errc::malformed_data);
    }
    return std::vector<std::uint32_t>(values.begin(), values.end());
}

inline result<std::vector<std::uint32_t>> parse_distance_list(
    std::string_view input) {
    input = trim_numa_field(input);
    if (input.empty()) { return std::vector<std::uint32_t>{}; }

    std::vector<std::uint32_t> distances;
    std::size_t offset = 0U;
    while (offset < input.size()) {
        while (offset < input.size() && numa_space(input[offset])) { ++offset; }
        if (offset >= input.size()) { break; }
        std::size_t end = offset;
        while (end < input.size() && !numa_space(input[end])) { ++end; }

        const std::string_view token = input.substr(offset, end - offset);
        std::uint32_t val = 0U;
        const std::from_chars_result parsed = std::from_chars(
            token.data(), token.data() + token.size(), val);
        if (parsed.ec != std::errc() || parsed.ptr != token.data() + token.size()) {
            return fail(errc::malformed_data);
        }
        distances.push_back(val);
        offset = end;
    }
    return distances;
}

struct node_meminfo {
    std::optional<std::uint64_t> total_bytes;
    std::optional<std::uint64_t> free_bytes;
    std::optional<std::uint64_t> used_bytes;
};

inline result<node_meminfo> parse_node_meminfo(std::string_view input) {
    node_meminfo info;
    std::size_t offset = 0U;
    while (offset < input.size()) {
        const std::size_t end = input.find('\n', offset);
        const std::string_view line = input.substr(
            offset, end == std::string_view::npos ? input.size() - offset
                                                   : end - offset);
        offset = end == std::string_view::npos ? input.size() : end + 1U;

        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos) { continue; }
        const std::string_view key = trim_numa_field(line.substr(0U, colon));
        const std::string_view value_str = trim_numa_field(line.substr(colon + 1U));

        auto parse_kb = [](std::string_view val_str) -> result<std::uint64_t> {
            val_str = trim_numa_field(val_str);
            if (val_str.empty()) { return fail(errc::malformed_data); }
            if (val_str.size() > 2U &&
                (val_str.substr(val_str.size() - 2U) == "kB" ||
                 val_str.substr(val_str.size() - 2U) == "KB")) {
                val_str = trim_numa_field(val_str.substr(0U, val_str.size() - 2U));
            }
            std::uint64_t kb = 0U;
            const std::from_chars_result parsed = std::from_chars(
                val_str.data(), val_str.data() + val_str.size(), kb);
            if (parsed.ec != std::errc() || parsed.ptr != val_str.data() + val_str.size()) {
                return fail(errc::malformed_data);
            }
            if (kb > std::numeric_limits<std::uint64_t>::max() / 1024ULL) {
                return fail(errc::malformed_data);
            }
            return kb * 1024ULL;
        };

        if (key.size() >= 8U && key.substr(key.size() - 8U) == "MemTotal") {
            const auto res = parse_kb(value_str);
            if (!res) { return fail(res.error()); }
            info.total_bytes = *res;
        } else if (key.size() >= 7U && key.substr(key.size() - 7U) == "MemFree") {
            const auto res = parse_kb(value_str);
            if (!res) { return fail(res.error()); }
            info.free_bytes = *res;
        } else if (key.size() >= 7U && key.substr(key.size() - 7U) == "MemUsed") {
            const auto res = parse_kb(value_str);
            if (!res) { return fail(res.error()); }
            info.used_bytes = *res;
        }
    }
    if (info.total_bytes && info.free_bytes && !info.used_bytes) {
        if (*info.total_bytes >= *info.free_bytes) {
            info.used_bytes = *info.total_bytes - *info.free_bytes;
        }
    }
    return info;
}

inline result<std::vector<std::uint32_t>> read_online_nodes_at(
    const char* node_root) {
    const std::string online_path = std::string(node_root) + "/online";
    const result<std::string> online_content =
        linux_platform::read_text_file(online_path.c_str(), 4096U);
    if (!online_content) { return fail(online_content.error()); }
    return parse_range_list(*online_content);
}

inline result<std::vector<std::uint32_t>> read_online_nodes() {
    return read_online_nodes_at("/sys/devices/system/node");
}

inline result<::syscape::numa::numa_node> read_single_node_at(
    std::uint32_t node_id,
    bool is_online,
    const char* node_root) {
    const std::string node_dir =
        std::string(node_root) + "/node" + std::to_string(node_id);

    if (::access(node_dir.c_str(), F_OK) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

    ::syscape::numa::numa_node node;
    node.id = node_id;
    node.is_online = is_online;

    // Read cpulist
    const result<std::string> cpulist_content =
        linux_platform::read_text_file((node_dir + "/cpulist").c_str(), 65536U);
    if (cpulist_content) {
        const std::string_view trimmed_cpu = trim_numa_field(*cpulist_content);
        if (!trimmed_cpu.empty()) {
            const auto cpus = parse_range_list(trimmed_cpu);
            if (!cpus) { return fail(cpus.error()); }
            node.logical_processors = *cpus;
        }
    } else if (cpulist_content.error() != std::errc::no_such_file_or_directory) {
        return fail(cpulist_content.error());
    }

    // Read meminfo
    const result<std::string> meminfo_content =
        linux_platform::read_text_file((node_dir + "/meminfo").c_str(), 65536U);
    if (meminfo_content) {
        const auto mem = parse_node_meminfo(*meminfo_content);
        if (!mem) { return fail(mem.error()); }
        node.total_memory_bytes = mem->total_bytes;
        node.free_memory_bytes = mem->free_bytes;
        node.used_memory_bytes = mem->used_bytes;
    } else if (meminfo_content.error() != std::errc::no_such_file_or_directory) {
        return fail(meminfo_content.error());
    }

    // Read distance
    const result<std::string> distance_content =
        linux_platform::read_text_file((node_dir + "/distance").c_str(), 16384U);
    if (distance_content) {
        const auto dist = parse_distance_list(*distance_content);
        if (!dist) { return fail(dist.error()); }
        node.distances = *dist;
    } else if (distance_content.error() != std::errc::no_such_file_or_directory) {
        return fail(distance_content.error());
    }

    return numa_common::validate_numa_node(std::move(node));
}

inline result<::syscape::numa::numa_node> read_single_node(
    std::uint32_t node_id,
    bool is_online) {
    return read_single_node_at(
        node_id, is_online, "/sys/devices/system/node");
}

inline result<::syscape::numa::numa_node> node_at(
    std::uint32_t id,
    const char* node_root) {
    const auto online_nodes = read_online_nodes_at(node_root);
    if (!online_nodes) { return fail(online_nodes.error()); }
    const bool is_online = std::binary_search(
        online_nodes->begin(), online_nodes->end(), id);
    return read_single_node_at(id, is_online, node_root);
}

inline result<bool> is_numa_available() {
    const auto online_nodes = read_online_nodes();
    if (!online_nodes) { return fail(online_nodes.error()); }
    return online_nodes->size() > 1U;
}

inline result<std::uint32_t> node_count() {
    const auto online_nodes = read_online_nodes();
    if (!online_nodes) { return fail(online_nodes.error()); }
    return static_cast<std::uint32_t>(online_nodes->size());
}

inline result<std::vector<::syscape::numa::numa_node>> nodes() {
    const auto online_nodes = read_online_nodes();
    if (!online_nodes) { return fail(online_nodes.error()); }

    std::vector<::syscape::numa::numa_node> result_nodes;
    result_nodes.reserve(online_nodes->size());
    for (std::uint32_t id : *online_nodes) {
        auto n = read_single_node(id, true);
        if (!n) { return fail(n.error()); }
        result_nodes.push_back(std::move(*n));
    }
    return numa_common::validate_numa_nodes(std::move(result_nodes));
}

inline result<::syscape::numa::numa_node> node(std::uint32_t id) {
    return node_at(id, "/sys/devices/system/node");
}

inline result<std::uint32_t> current_thread_node() {
#if defined(SYS_getcpu)
    unsigned int cpu_val = 0U;
    unsigned int node_val = 0U;
    long ret = ::syscall(SYS_getcpu, &cpu_val, &node_val, nullptr);
    if (ret == 0) {
        return static_cast<std::uint32_t>(node_val);
    }
#endif
    int sched_cpu = ::sched_getcpu();
    if (sched_cpu < 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    const auto all_nodes = nodes();
    if (!all_nodes) { return fail(all_nodes.error()); }
    for (const auto& n : *all_nodes) {
        if (std::binary_search(n.logical_processors.begin(),
                               n.logical_processors.end(),
                               static_cast<std::uint32_t>(sched_cpu))) {
            return n.id;
        }
    }
    return fail(errc::temporarily_unavailable);
}

} // namespace numa_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_NUMA_LINUX_HPP
