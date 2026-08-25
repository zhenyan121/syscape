#ifndef SYSCAPE_DETAIL_PROCESS_LIST_COMMON_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_COMMON_HPP

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string_view>
#include <vector>

namespace syscape {
namespace detail {
namespace process_list_common {

/// Case-insensitive ASCII string comparison.
inline bool equals_ignore_case(std::string_view left,
                               std::string_view right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(left[i])) !=
            std::tolower(static_cast<unsigned char>(right[i]))) {
            return false;
        }
    }
    return true;
}

/// Extracts the filename (basename) component from a path.
inline std::string_view extract_basename(std::string_view path) noexcept {
    const std::size_t slash_pos = path.find_last_of("/\\");
    if (slash_pos == std::string_view::npos) {
        return path;
    }
    return path.substr(slash_pos + 1U);
}

/// Checks if a process entry matches the specified name (exact name or basename).
inline bool matches_process_name(const process_list::process_entry& entry,
                                 std::string_view target,
                                 bool ignore_case) noexcept {
    if (target.empty()) {
        return false;
    }
    if (entry.name.has_value() &&
        (*entry.name == target ||
         (ignore_case && equals_ignore_case(*entry.name, target)))) {
        return true;
    }
    if (entry.executable_path.has_value()) {
        const std::string_view exe_base =
            extract_basename(*entry.executable_path);
        if (exe_base == target ||
            (ignore_case && equals_ignore_case(exe_base, target))) {
            return true;
        }
    }
    return false;
}

/// Sorts a vector of process entries in natural ascending order by PID.
inline void sort_processes(
    std::vector<process_list::process_entry>& list) noexcept {
    std::sort(list.begin(), list.end(),
              [](const process_list::process_entry& a,
                 const process_list::process_entry& b) noexcept {
                  return a.pid < b.pid;
              });
}

} // namespace process_list_common
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_PROCESS_LIST_COMMON_HPP
