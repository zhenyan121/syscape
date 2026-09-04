#ifndef SYSCAPE_DETAIL_PROCESS_LIST_AIX_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_AIX_HPP

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
#if __has_include(<procinfo.h>)
#include <procinfo.h>
#define SYSCAPE_HAS_AIX_PROCINFO 1
#endif
#endif

#include <syscape/detail/posix/passwd.hpp>
#include <syscape/detail/process_list/common.hpp>
#include <syscape/process_list.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_list_backend {

inline result<std::vector<process_list::process_entry>> processes() {
#if defined(SYSCAPE_HAS_AIX_PROCINFO)
    std::vector<process_list::process_entry> list;
    pid_t index = 0;
    struct procentry64 procs[64];
    int count = 0;
    while ((count = ::getprocs64(procs, sizeof(struct procentry64), nullptr, 0,
                                 &index, 64)) > 0) {
        for (int i = 0; i < count; ++i) {
#if defined(SNONE)
            if (procs[i].pi_state == SNONE) {
                continue;
            }
#endif
            process_list::process_entry entry;
            entry.pid = static_cast<std::uint32_t>(procs[i].pi_pid);
            if (procs[i].pi_ppid > 0) {
                entry.ppid = static_cast<std::uint32_t>(procs[i].pi_ppid);
            }
            entry.uid = static_cast<std::uint32_t>(procs[i].pi_uid);
            if (procs[i].pi_comm[0] != '\0') {
                entry.name = std::string(procs[i].pi_comm);
            }
            entry.thread_count =
                static_cast<std::uint32_t>(procs[i].pi_thcount);
            if (procs[i].pi_start > 0) {
                entry.start_time = std::chrono::system_clock::time_point(
                    std::chrono::seconds(procs[i].pi_start));
            }
            entry.resident_memory_bytes =
                static_cast<std::uint64_t>(procs[i].pi_drss +
                                           procs[i].pi_trss) *
                4096ULL;
            entry.virtual_memory_bytes =
                static_cast<std::uint64_t>(procs[i].pi_size) * 4096ULL;
            list.push_back(std::move(entry));
        }
    }
    process_list_common::sort_processes(list);
    return list;
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint32_t> process_count() {
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
#if defined(SYSCAPE_HAS_AIX_PROCINFO)
    struct procentry64 pe {};
    pid_t target_pid = static_cast<pid_t>(pid);
    if (::getprocs64(&pe, sizeof(pe), nullptr, 0, &target_pid, 1) > 0) {
        if (static_cast<std::uint32_t>(pe.pi_pid) != pid) {
            return fail(errc::not_found);
        }
#if defined(SNONE)
        if (pe.pi_state == SNONE) {
            return fail(errc::not_found);
        }
#endif
        process_list::process_entry entry;
        entry.pid = static_cast<std::uint32_t>(pe.pi_pid);
        if (pe.pi_ppid > 0) {
            entry.ppid = static_cast<std::uint32_t>(pe.pi_ppid);
        }
        entry.uid = static_cast<std::uint32_t>(pe.pi_uid);
        if (pe.pi_comm[0] != '\0') {
            entry.name = std::string(pe.pi_comm);
        }
        entry.thread_count = static_cast<std::uint32_t>(pe.pi_thcount);
        if (pe.pi_start > 0) {
            entry.start_time = std::chrono::system_clock::time_point(
                std::chrono::seconds(pe.pi_start));
        }
        entry.resident_memory_bytes =
            static_cast<std::uint64_t>(pe.pi_drss + pe.pi_trss) * 4096ULL;
        entry.virtual_memory_bytes =
            static_cast<std::uint64_t>(pe.pi_size) * 4096ULL;
        return entry;
    }
    return fail(errc::not_found);
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
        if (process_list_common::matches_process_name(proc, name, false)) {
            matches.push_back(proc);
        }
    }
    return matches;
}

} // namespace process_list_backend
} // namespace detail
} // namespace syscape

#endif
