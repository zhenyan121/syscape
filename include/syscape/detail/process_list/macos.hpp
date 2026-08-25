#ifndef SYSCAPE_DETAIL_PROCESS_LIST_MACOS_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_MACOS_HPP

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <pwd.h>
#include <string>
#include <string_view>
#include <sys/proc_info.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <libproc.h>

#include <syscape/detail/process_list/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_list_backend {

inline process_list::process_state macos_process_state(int state) noexcept {
    switch (state) {
    case SRUN:
        return process_list::process_state::running;
    case SSLEEP:
        return process_list::process_state::sleeping;
    case SSTOP:
        return process_list::process_state::stopped;
    case SZOMB:
        return process_list::process_state::zombie;
    default:
        return process_list::process_state::unknown;
    }
}

inline std::uint32_t macos_real_uid(const ::kinfo_proc& process) noexcept {
    return static_cast<std::uint32_t>(process.kp_eproc.e_pcred.p_ruid);
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

inline process_list::process_entry make_entry_from_kinfo(
    const ::kinfo_proc& kp) {
    process_list::process_entry entry;
    entry.pid = static_cast<std::uint32_t>(kp.kp_proc.p_pid);
    entry.ppid = static_cast<std::uint32_t>(kp.kp_eproc.e_ppid);
    entry.uid = macos_real_uid(kp);
    entry.gid = static_cast<std::uint32_t>(kp.kp_eproc.e_pcred.p_rgid);
    entry.priority = static_cast<int>(kp.kp_proc.p_nice);

    std::string comm(kp.kp_proc.p_comm);
    if (is_valid_utf8(comm)) {
        entry.name = std::move(comm);
    }

    entry.state = macos_process_state(kp.kp_proc.p_stat);

    // Start time
    const auto& st = kp.kp_proc.p_starttime;
    if (st.tv_sec > 0) {
        const auto secs = std::chrono::seconds(st.tv_sec);
        const auto micros = std::chrono::microseconds(st.tv_usec);
        entry.start_time =
            std::chrono::system_clock::time_point(secs + micros);
    }

    if (entry.uid.has_value()) {
        entry.user_name = lookup_username_by_uid(*entry.uid);
    }

    // Query task info for memory, cpu time, threads
    ::proc_taskinfo task_info{};
    const int task_bytes =
        ::proc_pidinfo(kp.kp_proc.p_pid, PROC_PIDTASKINFO, 0, &task_info,
                       sizeof(task_info));
    if (task_bytes == static_cast<int>(sizeof(task_info))) {
        entry.user_cpu_time =
            std::chrono::nanoseconds(task_info.pti_total_user);
        entry.kernel_cpu_time =
            std::chrono::nanoseconds(task_info.pti_total_system);
        entry.resident_memory_bytes = task_info.pti_resident_size;
        entry.virtual_memory_bytes = task_info.pti_virtual_size;
        entry.thread_count = static_cast<std::uint32_t>(task_info.pti_threadnum);
    }

    // Query executable path
    char path_buf[PROC_PIDPATHINFO_MAXSIZE];
    const int path_len =
        ::proc_pidpath(kp.kp_proc.p_pid, path_buf, sizeof(path_buf));
    if (path_len > 0) {
        std::string p(path_buf, static_cast<std::size_t>(path_len));
        if (is_valid_utf8(p)) {
            entry.executable_path = std::move(p);
        }
    }

    return entry;
}

inline result<std::vector<::kinfo_proc>> enumerate_kinfo_procs() {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    std::size_t size = 0;

    if (::sysctl(mib, 4, nullptr, &size, nullptr, 0) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }

    std::vector<::kinfo_proc> procs;
    for (;;) {
        const std::size_t count = size / sizeof(::kinfo_proc);
        procs.resize(count);

        if (::sysctl(mib, 4, procs.data(), &size, nullptr, 0) == 0) {
            procs.resize(size / sizeof(::kinfo_proc));
            return procs;
        }

        if (errno != ENOMEM) {
            return fail(std::error_code(errno, std::generic_category()));
        }

        if (size > (std::numeric_limits<std::size_t>::max)() / 3U) {
            return fail(errc::value_too_large);
        }
        size = (size * 3U) / 2U;
    }
}

inline result<std::vector<process_list::process_entry>> processes() {
    const auto kprocs = enumerate_kinfo_procs();
    if (!kprocs) {
        return fail(kprocs.error());
    }

    std::vector<process_list::process_entry> list;
    list.reserve(kprocs->size());

    for (const auto& kp : *kprocs) {
        if (kp.kp_proc.p_pid <= 0) {
            continue;
        }
        list.push_back(make_entry_from_kinfo(kp));
    }

    process_list_common::sort_processes(list);
    return list;
}

inline result<std::uint32_t> process_count() {
    const auto kprocs = enumerate_kinfo_procs();
    if (!kprocs) {
        return fail(kprocs.error());
    }

    std::size_t count = 0U;
    for (const auto& kp : *kprocs) {
        if (kp.kp_proc.p_pid > 0) {
            ++count;
        }
    }
    if (count > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(count);
}

inline result<process_list::process_entry> find_process(std::uint32_t pid) {
    if (pid == 0U ||
        pid > static_cast<std::uint32_t>(
                  (std::numeric_limits<int>::max)())) {
        return fail(errc::not_found);
    }

    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(pid)};
    ::kinfo_proc kp{};
    std::size_t size = sizeof(kp);

    if (::sysctl(mib, 4, &kp, &size, nullptr, 0) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0) {
        return fail(errc::not_found);
    }
    if (size != sizeof(kp)) {
        return fail(errc::malformed_data);
    }

    return make_entry_from_kinfo(kp);
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

#endif // SYSCAPE_DETAIL_PROCESS_LIST_MACOS_HPP
