#ifndef SYSCAPE_DETAIL_PROCESS_LIST_OPENBSD_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_OPENBSD_HPP

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <pwd.h>
#include <string>
#include <string_view>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/detail/process_list/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/process_list.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_list_backend {

inline result<std::chrono::nanoseconds>
timeval_to_nanoseconds(std::int64_t seconds, std::int64_t microseconds) {
    if (seconds < 0 || microseconds < 0 || microseconds >= 1000000) {
        return fail(errc::malformed_data);
    }
    constexpr std::uint64_t nanoseconds_per_second = 1000000000U;
    constexpr std::uint64_t nanoseconds_per_microsecond = 1000U;
    const std::uint64_t whole = static_cast<std::uint64_t>(seconds);
    const std::uint64_t fraction =
        static_cast<std::uint64_t>(microseconds) * nanoseconds_per_microsecond;
    const std::uint64_t maximum =
        static_cast<std::uint64_t>((std::chrono::nanoseconds::max)().count());
    if (whole > maximum / nanoseconds_per_second ||
        whole * nanoseconds_per_second > maximum - fraction) {
        return fail(errc::value_too_large);
    }
    return std::chrono::nanoseconds(whole * nanoseconds_per_second + fraction);
}

inline result<std::chrono::system_clock::time_point>
timeval_to_time_point(std::int64_t seconds, std::int64_t microseconds) {
    if (seconds < 0 || microseconds < 0 || microseconds >= 1000000) {
        return fail(errc::malformed_data);
    }
    using clock = std::chrono::system_clock;
    const std::int64_t maximum_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(clock::duration::max())
            .count();
    if (seconds > maximum_seconds) {
        return fail(errc::value_too_large);
    }
    const clock::duration whole = std::chrono::duration_cast<clock::duration>(
        std::chrono::seconds(seconds));
    const clock::duration fraction =
        std::chrono::duration_cast<clock::duration>(
            std::chrono::microseconds(microseconds));
    if (fraction > clock::duration::max() - whole) {
        return fail(errc::value_too_large);
    }
    return clock::time_point(whole + fraction);
}

inline process_list::process_state openbsd_process_state(char state) noexcept {
    switch (state) {
    case 2: // SRUN
    case 7: // SONPROC
        return process_list::process_state::running;
    case 1: // SIDL
    case 3: // SSLEEP
        return process_list::process_state::sleeping;
    case 4: // SSTOP
        return process_list::process_state::stopped;
    case 5: // SZOMB
    case 6: // SDEAD
        return process_list::process_state::zombie;
    default:
        return process_list::process_state::unknown;
    }
}

inline std::optional<std::string> lookup_username_by_uid(std::uint32_t uid) {
    constexpr std::size_t initial_size = 1024U;
    constexpr std::size_t maximum_size = 1024U * 1024U;
    std::vector<char> buffer(initial_size);

    for (;;) {
        ::passwd entry {};
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

inline result<process_list::process_entry>
make_entry_from_kinfo(const struct kinfo_proc& kp, std::uint64_t page_size) {
    process_list::process_entry entry;
    entry.pid = static_cast<std::uint32_t>(kp.p_pid);
    entry.ppid = static_cast<std::uint32_t>(kp.p_ppid);
    entry.uid = static_cast<std::uint32_t>(kp.p_ruid);
    entry.gid = static_cast<std::uint32_t>(kp.p_rgid);
    entry.user_name = lookup_username_by_uid(*entry.uid);

    std::string name(kp.p_comm);
    if (is_valid_utf8(name)) {
        entry.name = std::move(name);
    }

    entry.state = openbsd_process_state(kp.p_stat);
    if (kp.p_vm_rssize > 0 && page_size > 0) {
        constexpr std::uint64_t max_u64 =
            (std::numeric_limits<std::uint64_t>::max)();
        const std::uint64_t rss_pages =
            static_cast<std::uint64_t>(kp.p_vm_rssize);
        if (rss_pages > max_u64 / page_size) {
            return fail(errc::value_too_large);
        }
        entry.resident_memory_bytes = rss_pages * page_size;
    }
    if (kp.p_vm_map_size > 0) {
        entry.virtual_memory_bytes =
            static_cast<std::uint64_t>(kp.p_vm_map_size);
    }

    if (kp.p_uvalid != 0) {
        if (kp.p_ustart_sec >= 0 || kp.p_ustart_usec >= 0) {
            const auto st = timeval_to_time_point(
                static_cast<std::int64_t>(kp.p_ustart_sec),
                static_cast<std::int64_t>(kp.p_ustart_usec));
            if (!st) {
                return fail(st.error());
            }
            entry.start_time = *st;
        }

        if (kp.p_uutime_sec >= 0 || kp.p_uutime_usec >= 0) {
            const auto ut = timeval_to_nanoseconds(
                static_cast<std::int64_t>(kp.p_uutime_sec),
                static_cast<std::int64_t>(kp.p_uutime_usec));
            if (!ut) {
                return fail(ut.error());
            }
            entry.user_cpu_time = *ut;
        }

        if (kp.p_ustime_sec >= 0 || kp.p_ustime_usec >= 0) {
            const auto kt = timeval_to_nanoseconds(
                static_cast<std::int64_t>(kp.p_ustime_sec),
                static_cast<std::int64_t>(kp.p_ustime_usec));
            if (!kt) {
                return fail(kt.error());
            }
            entry.kernel_cpu_time = *kt;
        }
    }

    entry.thread_count = std::nullopt;
#ifndef NZERO
    constexpr int nzero = 20;
#else
    constexpr int nzero = NZERO;
#endif
    entry.priority = static_cast<int>(kp.p_nice) - nzero;

    return entry;
}

inline result<std::vector<struct kinfo_proc>> get_all_kinfo_procs() {
    constexpr int maximum_attempts = 4;
    int mib[6] = {CTL_KERN,
                  KERN_PROC,
                  KERN_PROC_ALL,
                  0,
                  static_cast<int>(sizeof(struct kinfo_proc)),
                  0};

    for (int attempt = 0; attempt < maximum_attempts; ++attempt) {
        std::size_t size = 0U;
        if (::sysctl(mib, 6, nullptr, &size, nullptr, 0U) != 0) {
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (size == 0U) {
            return std::vector<struct kinfo_proc>();
        }

        std::size_t count = size / sizeof(struct kinfo_proc);
        count += 16U;
        std::vector<struct kinfo_proc> procs(count);
        size = count * sizeof(struct kinfo_proc);

        mib[5] = static_cast<int>(count);
        if (::sysctl(mib, 6, procs.data(), &size, nullptr, 0U) != 0) {
            if (errno == ENOMEM) {
                continue;
            }
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (size % sizeof(struct kinfo_proc) != 0) {
            return fail(errc::malformed_data);
        }

        const std::size_t actual_count = size / sizeof(struct kinfo_proc);
        procs.resize(actual_count);
        return procs;
    }
    return fail(errc::temporarily_unavailable);
}

inline result<std::vector<process_list::process_entry>> processes() {
    const result<std::vector<struct kinfo_proc>> procs = get_all_kinfo_procs();
    if (!procs) {
        return fail(procs.error());
    }

    errno = 0;
    const long page_size_val = ::sysconf(_SC_PAGESIZE);
    if (page_size_val <= 0) {
        return errno != 0
                   ? result<std::vector<process_list::process_entry>>(
                         fail(std::error_code(errno, std::generic_category())))
                   : result<std::vector<process_list::process_entry>>(
                         fail(errc::malformed_data));
    }
    const std::uint64_t page_size = static_cast<std::uint64_t>(page_size_val);

    std::vector<process_list::process_entry> results;
    results.reserve(procs->size());

    for (const auto& kp : *procs) {
        if (kp.p_pid <= 0) {
            continue;
        }
        auto entry_res = make_entry_from_kinfo(kp, page_size);
        if (!entry_res) {
            return fail(entry_res.error());
        }
        results.push_back(std::move(*entry_res));
    }

    std::sort(results.begin(), results.end(),
              [](const process_list::process_entry& a,
                 const process_list::process_entry& b) noexcept {
                  return a.pid < b.pid;
              });

    return results;
}

inline result<std::uint32_t> process_count() {
    const result<std::vector<struct kinfo_proc>> procs = get_all_kinfo_procs();
    if (!procs) {
        return fail(procs.error());
    }
    std::uint32_t count = 0U;
    for (const auto& kp : *procs) {
        if (kp.p_pid > 0) {
            ++count;
        }
    }
    return count;
}

inline result<process_list::process_entry> find_process(std::uint32_t pid) {
    if (pid == 0U) {
        return fail(errc::not_found);
    }
    int mib[6] = {CTL_KERN,
                  KERN_PROC,
                  KERN_PROC_PID,
                  static_cast<int>(pid),
                  static_cast<int>(sizeof(struct kinfo_proc)),
                  1};
    struct kinfo_proc kp {};
    std::size_t size = sizeof(kp);
    if (::sysctl(mib, 6, &kp, &size, nullptr, 0U) != 0) {
        if (errno == ESRCH || errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size != sizeof(kp) || kp.p_pid <= 0) {
        return fail(errc::malformed_data);
    }

    errno = 0;
    const long page_size_val = ::sysconf(_SC_PAGESIZE);
    if (page_size_val <= 0) {
        return errno != 0
                   ? result<process_list::process_entry>(
                         fail(std::error_code(errno, std::generic_category())))
                   : result<process_list::process_entry>(
                         fail(errc::malformed_data));
    }
    const std::uint64_t page_size = static_cast<std::uint64_t>(page_size_val);
    return make_entry_from_kinfo(kp, page_size);
}

inline result<std::vector<process_list::process_entry>>
find_processes_by_name(std::string_view name) {
    if (name.empty()) {
        return std::vector<process_list::process_entry> {};
    }
    const result<std::vector<process_list::process_entry>> all = processes();
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
