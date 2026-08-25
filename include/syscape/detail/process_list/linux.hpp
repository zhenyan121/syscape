#ifndef SYSCAPE_DETAIL_PROCESS_LIST_LINUX_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_LINUX_HPP

#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <dirent.h>
#include <limits>
#include <optional>
#include <pwd.h>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/linux/process_metrics.hpp>
#include <syscape/detail/os/linux.hpp>
#include <syscape/detail/process_list/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_list_backend {

class dir_handle {
public:
    explicit dir_handle(::DIR* dir) noexcept : dir_(dir) {}
    dir_handle(const dir_handle&) = delete;
    dir_handle& operator=(const dir_handle&) = delete;
    ~dir_handle() {
        if (dir_ != nullptr) {
            static_cast<void>(::closedir(dir_));
        }
    }

    ::DIR* get() const noexcept { return dir_; }

private:
    ::DIR* dir_;
};

inline bool is_stat_separator(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
           c == '\v';
}

inline result<std::uint64_t> parse_u64_field(std::string_view token) noexcept {
    if (token.empty()) {
        return fail(errc::malformed_data);
    }
    std::uint64_t value = 0U;
    const char* first = token.data();
    const char* last = first + token.size();
    const std::from_chars_result parsed = std::from_chars(first, last, value);
    if (parsed.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (parsed.ec != std::errc() || parsed.ptr != last) {
        return fail(errc::malformed_data);
    }
    return value;
}

inline result<std::int64_t> parse_i64_field(std::string_view token) noexcept {
    if (token.empty()) {
        return fail(errc::malformed_data);
    }
    std::int64_t value = 0;
    const char* first = token.data();
    const char* last = first + token.size();
    const std::from_chars_result parsed = std::from_chars(first, last, value);
    if (parsed.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (parsed.ec != std::errc() || parsed.ptr != last) {
        return fail(errc::malformed_data);
    }
    return value;
}

inline std::optional<std::string> read_symlink_path(
    const std::string& link_path) {
    constexpr std::size_t initial_size = 256U;
    constexpr std::size_t maximum_size = 1024U * 1024U;
    std::vector<char> buffer(initial_size);

    for (;;) {
        errno = 0;
        const ssize_t count =
            ::readlink(link_path.c_str(), buffer.data(), buffer.size());
        if (count >= 0 && static_cast<std::size_t>(count) < buffer.size()) {
            std::string res(buffer.data(), static_cast<std::size_t>(count));
            if (!is_valid_utf8(res)) {
                return std::nullopt;
            }
            return res;
        }
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return std::nullopt;
        }
        if (buffer.size() > maximum_size / 2U) {
            return std::nullopt;
        }
        buffer.resize(buffer.size() * 2U);
    }
}

inline std::optional<std::string> lookup_username_by_uid(std::uint32_t uid) {
    constexpr std::size_t initial_size = 1024U;
    constexpr std::size_t maximum_size = 1024U * 1024U;
    std::vector<char> buffer(initial_size);

    for (;;) {
        ::passwd entry{};
        ::passwd* result_ptr = nullptr;
        const int outcome =
            ::getpwuid_r(static_cast<::uid_t>(uid), &entry, buffer.data(),
                         buffer.size(), &result_ptr);
        if (outcome != 0) {
            if (outcome == ERANGE && buffer.size() < maximum_size) {
                buffer.resize(buffer.size() <= maximum_size / 2U
                                  ? buffer.size() * 2U
                                  : maximum_size);
                continue;
            }
            return std::nullopt;
        }
        if (result_ptr == nullptr || result_ptr->pw_name == nullptr) {
            return std::nullopt;
        }
        std::string uname(result_ptr->pw_name);
        return is_valid_utf8(uname)
                   ? std::optional<std::string>(std::move(uname))
                   : std::nullopt;
    }
}

inline result<std::vector<std::string>> parse_proc_cmdline(
    std::string_view input) {
    if (input.empty()) {
        return std::vector<std::string>{};
    }
    if (input.back() != '\0') {
        return fail(errc::malformed_data);
    }
    std::vector<std::string> args;
    std::size_t start = 0U;
    while (start < input.size()) {
        const std::size_t end = input.find('\0', start);
        if (end == std::string_view::npos) {
            std::string arg(input.substr(start));
            if (!is_valid_utf8(arg)) {
                return fail(errc::invalid_encoding);
            }
            args.push_back(std::move(arg));
            break;
        }
        std::string arg(input.substr(start, end - start));
        if (!is_valid_utf8(arg)) {
            return fail(errc::invalid_encoding);
        }
        args.push_back(std::move(arg));
        start = end + 1U;
    }
    return args;
}

inline void parse_proc_status(std::string_view content,
                              process_list::process_entry& entry) {
    std::size_t offset = 0U;
    while (offset < content.size()) {
        const std::size_t newline = content.find('\n', offset);
        const std::string_view line =
            (newline == std::string_view::npos)
                ? content.substr(offset)
                : content.substr(offset, newline - offset);
        offset = (newline == std::string_view::npos) ? content.size()
                                                     : newline + 1U;

        if (line.size() >= 5U && line.substr(0U, 5U) == "Uid:\t") {
            const std::string_view rest = line.substr(5U);
            const std::size_t sp = rest.find_first_of(" \t");
            const std::string_view ruid_str =
                (sp == std::string_view::npos) ? rest : rest.substr(0U, sp);
            const auto parsed = parse_u64_field(ruid_str);
            if (parsed &&
                *parsed <= (std::numeric_limits<std::uint32_t>::max)()) {
                entry.uid = static_cast<std::uint32_t>(*parsed);
            }
        } else if (line.size() >= 5U && line.substr(0U, 5U) == "Gid:\t") {
            const std::string_view rest = line.substr(5U);
            const std::size_t sp = rest.find_first_of(" \t");
            const std::string_view rgid_str =
                (sp == std::string_view::npos) ? rest : rest.substr(0U, sp);
            const auto parsed = parse_u64_field(rgid_str);
            if (parsed &&
                *parsed <= (std::numeric_limits<std::uint32_t>::max)()) {
                entry.gid = static_cast<std::uint32_t>(*parsed);
            }
        } else if (!entry.name.has_value() && line.size() >= 6U &&
                   line.substr(0U, 6U) == "Name:\t") {
            std::string name_str(line.substr(6U));
            while (!name_str.empty() && (name_str.back() == '\r' ||
                                         name_str.back() == '\n' ||
                                         name_str.back() == ' ' ||
                                         name_str.back() == '\t')) {
                name_str.pop_back();
            }
            if (is_valid_utf8(name_str)) {
                entry.name = std::move(name_str);
            }
        }
    }
}

inline result<process_list::process_entry> parse_proc_stat_entry(
    std::string_view content, std::uint32_t pid, long ticks_per_sec,
    std::uint64_t page_size,
    const std::optional<std::chrono::system_clock::time_point>& boot_time) {
    const std::size_t name_start = content.find('(');
    const std::size_t name_end = content.rfind(')');
    if (name_start == std::string_view::npos ||
        name_end == std::string_view::npos || name_end < name_start) {
        return fail(errc::malformed_data);
    }

    process_list::process_entry entry;
    entry.pid = pid;
    std::string comm_str(content.substr(name_start + 1U, name_end - name_start - 1U));
    if (is_valid_utf8(comm_str)) {
        entry.name = std::move(comm_str);
    }

    const std::string_view remainder = content.substr(name_end + 1U);
    constexpr std::size_t max_fields = 22U;
    std::array<std::string_view, max_fields> fields{};
    std::size_t count = 0U;
    std::size_t pos = 0U;

    while (pos < remainder.size() && count < max_fields) {
        while (pos < remainder.size() && is_stat_separator(remainder[pos])) {
            ++pos;
        }
        if (pos >= remainder.size()) {
            break;
        }
        const std::size_t start = pos;
        while (pos < remainder.size() && !is_stat_separator(remainder[pos])) {
            ++pos;
        }
        fields[count++] = remainder.substr(start, pos - start);
    }

    if (count < max_fields) {
        return fail(errc::malformed_data);
    }

    // Field 3: state (index 0)
    if (!fields[0].empty()) {
        const char state_char = fields[0].front();
        switch (state_char) {
        case 'R':
            entry.state = process_list::process_state::running;
            break;
        case 'S':
        case 'D':
        case 'I':
        case 'K':
        case 'W':
        case 'P':
            entry.state = process_list::process_state::sleeping;
            break;
        case 'T':
        case 't':
            entry.state = process_list::process_state::stopped;
            break;
        case 'Z':
        case 'X':
        case 'x':
            entry.state = process_list::process_state::zombie;
            break;
        default:
            entry.state = process_list::process_state::unknown;
            break;
        }
    }

    // Field 4: ppid (index 1)
    const auto ppid_parsed = parse_u64_field(fields[1]);
    if (!ppid_parsed) {
        return fail(ppid_parsed.error());
    }
    if (*ppid_parsed > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    entry.ppid = static_cast<std::uint32_t>(*ppid_parsed);

    // Field 14: utime (index 11)
    const auto utime_parsed = parse_u64_field(fields[11]);
    if (!utime_parsed) {
        return fail(utime_parsed.error());
    }
    if (ticks_per_sec > 0) {
        const auto converted = linux_process_metrics::ticks_to_nanoseconds(
            *utime_parsed, ticks_per_sec);
        if (!converted) {
            return fail(converted.error());
        }
        entry.user_cpu_time = *converted;
    }

    // Field 15: stime (index 12)
    const auto stime_parsed = parse_u64_field(fields[12]);
    if (!stime_parsed) {
        return fail(stime_parsed.error());
    }
    if (ticks_per_sec > 0) {
        const auto converted = linux_process_metrics::ticks_to_nanoseconds(
            *stime_parsed, ticks_per_sec);
        if (!converted) {
            return fail(converted.error());
        }
        entry.kernel_cpu_time = *converted;
    }

    // Field 18: priority (index 15)
    const auto pri_parsed = parse_i64_field(fields[15]);
    if (!pri_parsed) {
        return fail(pri_parsed.error());
    }
    if (*pri_parsed < (std::numeric_limits<int>::min)() ||
        *pri_parsed > (std::numeric_limits<int>::max)()) {
        return fail(errc::value_too_large);
    }
    entry.priority = static_cast<int>(*pri_parsed);

    // Field 20: num_threads (index 17)
    const auto threads_parsed = parse_u64_field(fields[17]);
    if (!threads_parsed) {
        return fail(threads_parsed.error());
    }
    if (*threads_parsed > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    entry.thread_count = static_cast<std::uint32_t>(*threads_parsed);

    // Field 22: starttime (index 19)
    const auto starttime_parsed = parse_u64_field(fields[19]);
    if (!starttime_parsed) {
        return fail(starttime_parsed.error());
    }
    if (boot_time.has_value() && ticks_per_sec > 0) {
        const auto age = linux_process_metrics::ticks_to_nanoseconds(
            *starttime_parsed, ticks_per_sec);
        if (!age) {
            return fail(age.error());
        }
        const auto composed =
            linux_process_metrics::compose_start_time(*boot_time, *age);
        if (!composed) {
            return fail(composed.error());
        }
        entry.start_time = *composed;
    }

    // Field 23: vsize (index 20)
    const auto vsize_parsed = parse_u64_field(fields[20]);
    if (!vsize_parsed) {
        return fail(vsize_parsed.error());
    }
    entry.virtual_memory_bytes = *vsize_parsed;

    // Field 24: rss (index 21) in pages
    const auto rss_parsed = parse_u64_field(fields[21]);
    if (!rss_parsed) {
        return fail(rss_parsed.error());
    }
    if (page_size > 0U) {
        const auto scaled = linux_process_metrics::scale_resident_bytes(
            *rss_parsed, page_size);
        if (!scaled) {
            return fail(scaled.error());
        }
        entry.resident_memory_bytes = *scaled;
    }

    return entry;
}

inline result<process_list::process_entry> query_single_process(
    std::uint32_t pid, long ticks_per_sec, std::uint64_t page_size,
    const std::optional<std::chrono::system_clock::time_point>& boot_time) {
    const std::string pid_str = std::to_string(pid);
    const std::string stat_path = "/proc/" + pid_str + "/stat";

    const result<std::string> stat_content =
        linux_platform::read_text_file(stat_path.c_str());
    if (!stat_content) {
        if (stat_content.error() == std::errc::no_such_file_or_directory ||
            stat_content.error() == errc::not_found) {
            return fail(errc::not_found);
        }
        return fail(stat_content.error());
    }

    result<process_list::process_entry> entry_res = parse_proc_stat_entry(
        *stat_content, pid, ticks_per_sec, page_size, boot_time);
    if (!entry_res) {
        return fail(entry_res.error());
    }

    process_list::process_entry& entry = *entry_res;

    // Read /proc/[pid]/status
    const std::string status_path = "/proc/" + pid_str + "/status";
    const result<std::string> status_content =
        linux_platform::read_text_file(status_path.c_str());
    if (status_content) {
        parse_proc_status(*status_content, entry);
    }

    // Resolve username from uid if available
    if (entry.uid.has_value()) {
        entry.user_name = lookup_username_by_uid(*entry.uid);
    }

    // Read /proc/[pid]/cmdline
    const std::string cmdline_path = "/proc/" + pid_str + "/cmdline";
    const result<std::string> cmdline_content =
        linux_platform::read_text_file(cmdline_path.c_str());
    if (cmdline_content) {
        auto parsed_command_line = parse_proc_cmdline(*cmdline_content);
        if (parsed_command_line) {
            entry.command_line = std::move(*parsed_command_line);
        } else if (parsed_command_line.error() != errc::invalid_encoding &&
                   parsed_command_line.error() != errc::malformed_data) {
            return fail(parsed_command_line.error());
        }
    }

    // Read /proc/[pid]/exe
    const std::string exe_path = "/proc/" + pid_str + "/exe";
    entry.executable_path = read_symlink_path(exe_path);

    // Read /proc/[pid]/cwd
    const std::string cwd_path = "/proc/" + pid_str + "/cwd";
    entry.working_directory = read_symlink_path(cwd_path);

    return entry;
}

inline bool skippable_process_error(const std::error_code& error) noexcept {
    return error == errc::not_found ||
           error == std::errc::permission_denied ||
           error == errc::malformed_data ||
           error == errc::value_too_large;
}

inline result<std::vector<process_list::process_entry>> processes() {
    ::DIR* dir = ::opendir("/proc");
    if (dir == nullptr) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    const dir_handle guard(dir);

    const long ticks = ::sysconf(_SC_CLK_TCK);
    const long page_sz = ::sysconf(_SC_PAGESIZE);
    const std::uint64_t page_size =
        page_sz > 0 ? static_cast<std::uint64_t>(page_sz) : 0U;
    if (ticks > 0) {
        const auto rate_validation =
            linux_process_metrics::ticks_to_nanoseconds(0U, ticks);
        if (!rate_validation) {
            return fail(rate_validation.error());
        }
    }

    std::optional<std::chrono::system_clock::time_point> boot_tp;
    const auto boot_res = os_backend::boot_time();
    if (boot_res) {
        boot_tp = *boot_res;
    }

    std::vector<process_list::process_entry> result_list;

    for (;;) {
        errno = 0;
        ::dirent* ent = ::readdir(dir);
        if (ent == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }

        if (ent->d_name[0] < '0' || ent->d_name[0] > '9') {
            continue;
        }

        std::uint32_t pid = 0U;
        const char* first = ent->d_name;
        const char* last = first;
        while (*last != '\0') {
            ++last;
        }
        const auto parse_res = std::from_chars(first, last, pid);
        if (parse_res.ec != std::errc() || parse_res.ptr != last || pid == 0U) {
            continue;
        }

        auto entry_res =
            query_single_process(pid, ticks, page_size, boot_tp);
        if (entry_res) {
            result_list.push_back(std::move(*entry_res));
        } else if (!skippable_process_error(entry_res.error())) {
            return fail(entry_res.error());
        }
    }

    process_list_common::sort_processes(result_list);
    return result_list;
}

inline result<std::uint32_t> process_count() {
    const auto list = processes();
    if (!list) {
        return fail(list.error());
    }
    if (list->size() > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(list->size());
}

inline result<process_list::process_entry> find_process(std::uint32_t pid) {
    if (pid == 0U) {
        return fail(errc::not_found);
    }

    const long ticks = ::sysconf(_SC_CLK_TCK);
    const long page_sz = ::sysconf(_SC_PAGESIZE);
    const std::uint64_t page_size =
        page_sz > 0 ? static_cast<std::uint64_t>(page_sz) : 0U;
    if (ticks > 0) {
        const auto rate_validation =
            linux_process_metrics::ticks_to_nanoseconds(0U, ticks);
        if (!rate_validation) {
            return fail(rate_validation.error());
        }
    }

    std::optional<std::chrono::system_clock::time_point> boot_tp;
    const auto boot_res = os_backend::boot_time();
    if (boot_res) {
        boot_tp = *boot_res;
    }

    return query_single_process(pid, ticks, page_size, boot_tp);
}

inline result<std::vector<process_list::process_entry>> find_processes_by_name(
    std::string_view name) {
    if (name.empty()) {
        return std::vector<process_list::process_entry>{};
    }
    const auto all = processes();
    if (!all) {
        return fail(all.error());
    }

    std::vector<process_list::process_entry> filtered;
    for (const auto& entry : *all) {
        if (process_list_common::matches_process_name(entry, name, false)) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}

} // namespace process_list_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_PROCESS_LIST_LINUX_HPP
