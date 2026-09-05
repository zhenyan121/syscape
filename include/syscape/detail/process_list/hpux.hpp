#ifndef SYSCAPE_DETAIL_PROCESS_LIST_HPUX_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_HPUX_HPP

#include <syscape/detail/config.hpp>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#if defined(__has_include)
#if __has_include(<sys/pstat.h>)
#include <sys/pstat.h>
#define SYSCAPE_HAS_HPUX_PSTAT 1
#endif
#endif

#include <syscape/detail/posix/passwd.hpp>
#include <syscape/detail/process_list/common.hpp>
#include <syscape/process_list.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_list_backend {

inline std::optional<std::chrono::system_clock::time_point>
start_time_from_seconds(std::uint64_t seconds) {
    using clock = std::chrono::system_clock;
    const auto maximum_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(clock::duration::max())
            .count();
    if (seconds > static_cast<std::uint64_t>(maximum_seconds)) {
        return std::nullopt;
    }
    return clock::time_point(std::chrono::duration_cast<clock::duration>(
        std::chrono::seconds(seconds)));
}

#if defined(SYSCAPE_HAS_HPUX_PSTAT)
template <typename Integer>
inline bool try_add_page_count(std::uint64_t& total, Integer pages) {
    if (pages < 0) {
        return false;
    }
    const auto value = static_cast<std::uint64_t>(pages);
    if (value > UINT64_MAX - total) {
        return false;
    }
    total += value;
    return true;
}

inline void populate_memory_fields(process_list::process_entry& entry,
                                   const struct pst_status& process,
                                   std::uint64_t page_size) {
    if (page_size == 0U) {
        return;
    }

    std::uint64_t resident_pages = 0U;
    if (try_add_page_count(resident_pages, process.pst_rssize) &&
        try_add_page_count(resident_pages, process.pst_shmsize) &&
        try_add_page_count(resident_pages, process.pst_mmsize) &&
        try_add_page_count(resident_pages, process.pst_usize) &&
        try_add_page_count(resident_pages, process.pst_iosize) &&
        resident_pages <= UINT64_MAX / page_size) {
        entry.resident_memory_bytes = resident_pages * page_size;
    }

    std::uint64_t virtual_pages = 0U;
    if (try_add_page_count(virtual_pages, process.pst_vtsize) &&
        try_add_page_count(virtual_pages, process.pst_vdsize) &&
        try_add_page_count(virtual_pages, process.pst_vssize) &&
        try_add_page_count(virtual_pages, process.pst_vshmsize) &&
        try_add_page_count(virtual_pages, process.pst_vmmsize) &&
        try_add_page_count(virtual_pages, process.pst_vusize) &&
        try_add_page_count(virtual_pages, process.pst_viosize)
#if defined(__ia64) || defined(__ia64__)
        && try_add_page_count(virtual_pages, process.pst_vrsesize)
#endif
        && virtual_pages <= UINT64_MAX / page_size) {
        entry.virtual_memory_bytes = virtual_pages * page_size;
    }
}

inline result<std::uint32_t> checked_identity(_T_LONG_T value) {
    if (value < 0) {
        return fail(errc::malformed_data);
    }
    const auto converted = static_cast<std::uint64_t>(value);
    if (converted > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(converted);
}
#endif

inline result<std::vector<process_list::process_entry>> processes() {
#if defined(SYSCAPE_HAS_HPUX_PSTAT)
    struct pst_static pst {};
    std::uint64_t page_size = 0U;
    if (::pstat_getstatic(&pst, sizeof(pst), 1, 0) == 1 && pst.page_size > 0) {
        page_size = static_cast<std::uint64_t>(pst.page_size);
    } else {
        const long ps = ::sysconf(_SC_PAGESIZE);
        if (ps > 0) {
            page_size = static_cast<std::uint64_t>(ps);
        }
    }

    std::vector<process_list::process_entry> list;
    int index = 0;
    struct pst_status procs[64];
    while (true) {
        errno = 0;
        const int count =
            ::pstat_getproc(procs, sizeof(struct pst_status), 64, index);
        if (count < 0) {
            const int saved_errno = errno;
            if (saved_errno == EACCES || saved_errno == EPERM) {
                return fail(errc::permission_denied);
            }
            if (saved_errno != 0) {
                return fail(
                    std::error_code(saved_errno, std::generic_category()));
            }
            return fail(errc::io_error);
        }
        if (count == 0) {
            break;
        }
        if (count > 64) {
            return fail(errc::malformed_data);
        }
        for (int i = 0; i < count; ++i) {
            const auto pid = checked_identity(procs[i].pst_pid);
            const auto uid = checked_identity(procs[i].pst_uid);
            const auto gid = checked_identity(procs[i].pst_gid);
            if (!pid) {
                return fail(pid.error());
            }
            if (!uid) {
                return fail(uid.error());
            }
            if (!gid) {
                return fail(gid.error());
            }
            if (procs[i].pst_ppid < 0) {
                return fail(errc::malformed_data);
            }
            process_list::process_entry entry;
            entry.pid = *pid;
            if (procs[i].pst_ppid > 0) {
                const auto ppid = checked_identity(procs[i].pst_ppid);
                if (!ppid) {
                    return fail(ppid.error());
                }
                entry.ppid = *ppid;
            }
            entry.uid = *uid;
            entry.gid = *gid;
            if (procs[i].pst_ucomm[0] != '\0') {
                entry.name = std::string(procs[i].pst_ucomm);
            }
            entry.thread_count = std::nullopt;
            if (procs[i].pst_start > 0) {
                entry.start_time = start_time_from_seconds(
                    static_cast<std::uint64_t>(procs[i].pst_start));
            }
            populate_memory_fields(entry, procs[i], page_size);
            list.push_back(std::move(entry));
        }
        const auto last_index = procs[count - 1].pst_idx;
        if (last_index < 0) {
            return fail(errc::malformed_data);
        }
        if (last_index < static_cast<_T_LONG_T>(index)) {
            return fail(errc::malformed_data);
        }
        if (last_index >= (std::numeric_limits<int>::max)()) {
            return fail(errc::value_too_large);
        }
        index = static_cast<int>(last_index) + 1;
    }
    process_list_common::sort_processes(list);
    return list;
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint32_t> process_count() {
    const auto procs = processes();
    if (!procs) {
        return fail(procs.error());
    }
    if (procs->size() >
        static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)())) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(procs->size());
}

inline result<process_list::process_entry> find_process(std::uint32_t pid) {
    if (pid == 0U) {
        return fail(errc::not_found);
    }
#if defined(SYSCAPE_HAS_HPUX_PSTAT)
    if (pid > static_cast<std::uint32_t>((std::numeric_limits<int>::max)())) {
        return fail(errc::value_too_large);
    }
    struct pst_static pst {};
    std::uint64_t page_size = 0U;
    if (::pstat_getstatic(&pst, sizeof(pst), 1, 0) == 1 && pst.page_size > 0) {
        page_size = static_cast<std::uint64_t>(pst.page_size);
    } else {
        const long ps = ::sysconf(_SC_PAGESIZE);
        if (ps > 0) {
            page_size = static_cast<std::uint64_t>(ps);
        }
    }

    struct pst_status pe {};
    errno = 0;
    const int count =
        ::pstat_getproc(&pe, sizeof(pe), 0, static_cast<pid_t>(pid));
    if (count == 1) {
        const auto returned_pid = checked_identity(pe.pst_pid);
        if (!returned_pid) {
            return fail(returned_pid.error());
        }
        if (*returned_pid != pid) {
            return fail(errc::not_found);
        }
        const auto uid = checked_identity(pe.pst_uid);
        const auto gid = checked_identity(pe.pst_gid);
        if (!uid) {
            return fail(uid.error());
        }
        if (!gid) {
            return fail(gid.error());
        }
        if (pe.pst_ppid < 0) {
            return fail(errc::malformed_data);
        }
        process_list::process_entry entry;
        entry.pid = *returned_pid;
        if (pe.pst_ppid > 0) {
            const auto ppid = checked_identity(pe.pst_ppid);
            if (!ppid) {
                return fail(ppid.error());
            }
            entry.ppid = *ppid;
        }
        entry.uid = *uid;
        entry.gid = *gid;
        if (pe.pst_ucomm[0] != '\0') {
            entry.name = std::string(pe.pst_ucomm);
        }
        entry.thread_count = std::nullopt;
        if (pe.pst_start > 0) {
            entry.start_time = start_time_from_seconds(
                static_cast<std::uint64_t>(pe.pst_start));
        }
        populate_memory_fields(entry, pe, page_size);
        return entry;
    }
    if (count > 1) {
        return fail(errc::malformed_data);
    }
    const int saved_errno = errno;
    if (count == 0) {
        return fail(errc::not_found);
    }
    if (saved_errno == ESRCH || saved_errno == ENOENT) {
        return fail(errc::not_found);
    }
    if (saved_errno == EACCES || saved_errno == EPERM) {
        return fail(errc::permission_denied);
    }
    if (saved_errno != 0) {
        return fail(std::error_code(saved_errno, std::generic_category()));
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
    for (const auto& p : *all) {
        if (p.name && *p.name == name) {
            matches.push_back(p);
        }
    }
    return matches;
}

} // namespace process_list_backend
} // namespace detail
} // namespace syscape

#endif
