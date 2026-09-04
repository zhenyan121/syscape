#ifndef SYSCAPE_DETAIL_PROCESS_LIST_HAIKU_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_HAIKU_HPP

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(__has_include)
#if __has_include(<OS.h>)
#include <OS.h>
#define SYSCAPE_HAS_HAIKU_OS_H 1
#endif
#if __has_include(<image.h>)
#include <image.h>
#define SYSCAPE_HAS_HAIKU_IMAGE_H 1
#endif
#endif

#include <syscape/detail/haiku/error.hpp>
#include <syscape/detail/posix/passwd.hpp>
#include <syscape/detail/process_list/common.hpp>
#include <syscape/process_list.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_list_backend {

#if defined(SYSCAPE_HAS_HAIKU_OS_H)
inline process_list::process_entry convert_team(const ::team_info& tinfo,
                                                bigtime_t boot_time) {
    process_list::process_entry entry;
    entry.pid = static_cast<std::uint32_t>(tinfo.team);
    if (tinfo.parent > 0) {
        entry.ppid = static_cast<std::uint32_t>(tinfo.parent);
    }
    entry.uid = static_cast<std::uint32_t>(tinfo.real_uid);
    entry.gid = static_cast<std::uint32_t>(tinfo.real_gid);
    if (tinfo.name[0] != '\0') {
        entry.name = std::string(tinfo.name);
    }

#if defined(SYSCAPE_HAS_HAIKU_IMAGE_H)
    int32 icookie = 0;
    ::image_info iinfo {};
    while (::get_next_image_info(tinfo.team, &icookie, &iinfo) == B_OK) {
        if (iinfo.type == B_APP_IMAGE && iinfo.name[0] == '/') {
            entry.executable_path = std::string(iinfo.name);
            break;
        }
    }
#endif

    // Team arguments are stored as a space-separated 64-byte truncated string
    // without parameter boundaries, so command_line cannot be faithfully
    // reconstructed.
    entry.command_line = std::nullopt;

    entry.thread_count = static_cast<std::uint32_t>(tinfo.thread_count);
    if (boot_time > 0 && tinfo.start_time > 0) {
        const bigtime_t wall_start_us = boot_time + tinfo.start_time;
        const auto us = std::chrono::microseconds(wall_start_us);
        entry.start_time = std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                us));
    }
    entry.state = process_list::process_state::unknown;

    const auto pwd_entry = posix_passwd::entry_by_uid(tinfo.real_uid);
    if (pwd_entry && !pwd_entry->name.empty()) {
        entry.user_name = pwd_entry->name;
    }

    ::team_usage_info uinfo {};
    if (::get_team_usage_info(tinfo.team, B_TEAM_USAGE_SELF, &uinfo) == B_OK) {
        entry.user_cpu_time =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::microseconds(uinfo.user_time));
        entry.kernel_cpu_time =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::microseconds(uinfo.kernel_time));
    }

    ssize_t acookie = 0;
    ::area_info ainfo {};
    std::uint64_t ram = 0;
    std::uint64_t virt = 0;
    while (::get_next_area_info(tinfo.team, &acookie, &ainfo) == B_OK) {
        ram += ainfo.ram_size;
        virt += ainfo.size;
    }
    if (ram > 0) {
        entry.resident_memory_bytes = ram;
    }
    if (virt > 0) {
        entry.virtual_memory_bytes = virt;
    }

    return entry;
}
#endif

inline result<std::vector<process_list::process_entry>> processes() {
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::system_info sinfo {};
    bigtime_t boot_time = 0;
    const status_t s_st = ::get_system_info(&sinfo);
    if (s_st == B_OK) {
        boot_time = sinfo.boot_time;
    } else {
        return fail(haiku_error::make_haiku_error(s_st));
    }

    std::vector<process_list::process_entry> list;
    int32 cookie = 0;
    ::team_info tinfo {};
    status_t st = B_OK;
    while ((st = ::get_next_team_info(&cookie, &tinfo)) == B_OK) {
        if (tinfo.team <= 0) {
            continue;
        }
        list.push_back(convert_team(tinfo, boot_time));
    }
    if (!haiku_error::is_iteration_end(st)) {
        return fail(haiku_error::make_haiku_error(st));
    }
    std::sort(list.begin(), list.end(),
              [](const auto& a, const auto& b) { return a.pid < b.pid; });
    return list;
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint32_t> process_count() {
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::system_info sinfo {};
    const status_t st = ::get_system_info(&sinfo);
    if (st == B_OK && sinfo.used_teams > 0) {
        return static_cast<std::uint32_t>(sinfo.used_teams);
    }
    if (st != B_OK && st != B_UNSUPPORTED) {
        return fail(haiku_error::make_haiku_error(st));
    }
#endif
    const auto procs = processes();
    if (procs) {
        return static_cast<std::uint32_t>(procs->size());
    }
    return fail(errc::not_supported);
}

inline result<process_list::process_entry> find_process(std::uint32_t pid) {
    if (pid == 0U) {
        return fail(errc::not_found);
    }
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::team_info tinfo {};
    const status_t t_st = ::get_team_info(static_cast<team_id>(pid), &tinfo);
    if (t_st == B_OK) {
        ::system_info sinfo {};
        bigtime_t boot_time = 0;
        if (::get_system_info(&sinfo) == B_OK) {
            boot_time = sinfo.boot_time;
        }
        return convert_team(tinfo, boot_time);
    }
    return fail(haiku_error::make_haiku_error(t_st));
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::vector<process_list::process_entry>>
find_processes_by_name(std::string_view name) {
    const auto all = processes();
    if (!all) {
        return fail(all.error());
    }
    std::vector<process_list::process_entry> matches;
    for (const auto& proc : *all) {
        if (proc.name && *proc.name == name) {
            matches.push_back(proc);
        }
    }
    return matches;
}

} // namespace process_list_backend
} // namespace detail
} // namespace syscape

#endif
