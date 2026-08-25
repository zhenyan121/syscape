#ifndef SYSCAPE_DETAIL_PROCESS_LIST_WINDOWS_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_WINDOWS_HPP

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0601
#error "syscape/process_list.hpp requires _WIN32_WINNT >= 0x0601 on Windows"
#endif

#if defined(WINVER) && WINVER < 0x0601
#error "syscape/process_list.hpp requires WINVER >= 0x0601 on Windows"
#endif

#if !defined(_WIN32_WINNT)
#define SYSCAPE_DETAIL_PROCESS_LIST_DEFINED_WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#if !defined(WINVER)
#define SYSCAPE_DETAIL_PROCESS_LIST_DEFINED_WINVER
#define WINVER 0x0601
#endif

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <syscape/detail/process_list/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_list_backend {

class handle_guard {
public:
    explicit handle_guard(::HANDLE handle) noexcept : handle_(handle) {}
    handle_guard(const handle_guard&) = delete;
    handle_guard& operator=(const handle_guard&) = delete;
    ~handle_guard() {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
            static_cast<void>(::CloseHandle(handle_));
        }
    }

    ::HANDLE get() const noexcept { return handle_; }
    bool is_valid() const noexcept {
        return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
    }

private:
    ::HANDLE handle_;
};

inline result<std::string> wide_to_utf8(std::wstring_view value) {
    static_assert(sizeof(wchar_t) == sizeof(char16_t),
                  "The Windows backend requires 16-bit wchar_t");
    std::u16string converted;
    converted.reserve(value.size());
    for (wchar_t unit : value) {
        converted.push_back(static_cast<char16_t>(unit));
    }
    return utf16_to_utf8(converted);
}

inline std::chrono::nanoseconds filetime_to_duration(
    const ::FILETIME& ft) noexcept {
    const std::uint64_t ticks =
        (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32U) |
        static_cast<std::uint64_t>(ft.dwLowDateTime);
    // FILETIME is in 100-nanosecond intervals
    constexpr std::uint64_t max_ticks = static_cast<std::uint64_t>(
        (std::chrono::nanoseconds::max)().count() / 100);
    if (ticks > max_ticks) {
        return (std::chrono::nanoseconds::max)();
    }
    return std::chrono::nanoseconds(ticks * 100ULL);
}

inline std::optional<std::chrono::system_clock::time_point>
filetime_to_system_clock(const ::FILETIME& ft) noexcept {
    const std::uint64_t ticks =
        (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32U) |
        static_cast<std::uint64_t>(ft.dwLowDateTime);
    if (ticks == 0U) {
        return std::nullopt;
    }
    // Difference between Windows epoch (1601-01-01) and Unix epoch (1970-01-01) in 100ns units
    constexpr std::uint64_t epoch_offset = 116444736000000000ULL;
    if (ticks < epoch_offset) {
        return std::nullopt;
    }
    const std::uint64_t unix_100ns = ticks - epoch_offset;
    const std::uint64_t max_unix_100ns = static_cast<std::uint64_t>(
        (std::chrono::nanoseconds::max)().count() / 100);
    if (unix_100ns > max_unix_100ns) {
        return std::nullopt;
    }
    const std::uint64_t unix_nanos = unix_100ns * 100ULL;
    return std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::nanoseconds(unix_nanos)));
}

inline std::optional<std::string> query_process_username(::HANDLE hProcess) {
    ::HANDLE raw_token = nullptr;
    if (!::OpenProcessToken(hProcess, TOKEN_QUERY, &raw_token)) {
        return std::nullopt;
    }
    const handle_guard token_guard(raw_token);

    ::DWORD length = 0;
    ::GetTokenInformation(raw_token, TokenUser, nullptr, 0, &length);
    if (length == 0) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> buffer(length);
    if (!::GetTokenInformation(raw_token, TokenUser, buffer.data(), length,
                               &length)) {
        return std::nullopt;
    }

    const auto* token_user =
        reinterpret_cast<const ::TOKEN_USER*>(buffer.data());
    if (token_user == nullptr || token_user->User.Sid == nullptr) {
        return std::nullopt;
    }

    ::DWORD name_size = 0;
    ::DWORD domain_size = 0;
    ::SID_NAME_USE sid_use;
    static_cast<void>(::LookupAccountSidW(nullptr, token_user->User.Sid,
                                          nullptr, &name_size, nullptr,
                                          &domain_size, &sid_use));
    if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER || name_size == 0U) {
        return std::nullopt;
    }

    std::vector<wchar_t> name_buf(name_size);
    std::vector<wchar_t> domain_buf(domain_size);

    if (!::LookupAccountSidW(nullptr, token_user->User.Sid, name_buf.data(),
                             &name_size, domain_buf.data(), &domain_size,
                             &sid_use)) {
        return std::nullopt;
    }

    const auto res = wide_to_utf8(std::wstring_view(name_buf.data()));
    return res ? std::optional<std::string>(*res) : std::nullopt;
}

inline std::optional<std::string> query_process_path(::HANDLE process) {
    constexpr std::size_t maximum_path_units = 32768U;
    std::vector<wchar_t> buffer(512U);

    for (;;) {
        ::DWORD path_size = static_cast<::DWORD>(buffer.size());
        if (::QueryFullProcessImageNameW(process, 0, buffer.data(),
                                         &path_size)) {
            const auto path = wide_to_utf8(
                std::wstring_view(buffer.data(), path_size));
            return path ? std::optional<std::string>(*path) : std::nullopt;
        }
        if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
            buffer.size() >= maximum_path_units) {
            return std::nullopt;
        }
        buffer.resize((std::min)(buffer.size() * 2U, maximum_path_units));
    }
}

inline process_list::process_entry make_entry_from_snapshot(
    const ::PROCESSENTRY32W& pe) {
    process_list::process_entry entry;
    entry.pid = pe.th32ProcessID;
    entry.ppid = pe.th32ParentProcessID;
    entry.thread_count = pe.cntThreads;
    entry.priority = static_cast<int>(pe.pcPriClassBase);

    const auto name_utf8 = wide_to_utf8(pe.szExeFile);
    if (name_utf8) {
        entry.name = *name_utf8;
    }

    // Try to open the process for extended metadata
    ::HANDLE hProcess =
        ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
                      FALSE, pe.th32ProcessID);
    if (hProcess == nullptr) {
        hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                 pe.th32ProcessID);
    }

    if (hProcess != nullptr) {
        const handle_guard proc_guard(hProcess);

        // CPU times & start time
        ::FILETIME creation{}, exit{}, kernel{}, user{};
        if (::GetProcessTimes(hProcess, &creation, &exit, &kernel, &user)) {
            entry.start_time = filetime_to_system_clock(creation);
            entry.user_cpu_time = filetime_to_duration(user);
            entry.kernel_cpu_time = filetime_to_duration(kernel);
        }

        // Memory usage
        ::PROCESS_MEMORY_COUNTERS_EX pmc{};
        pmc.cb = sizeof(pmc);
        if (::GetProcessMemoryInfo(
                hProcess, reinterpret_cast<::PROCESS_MEMORY_COUNTERS*>(&pmc),
                sizeof(pmc))) {
            entry.resident_memory_bytes = pmc.WorkingSetSize;
        }

        // Executable path
        entry.executable_path = query_process_path(hProcess);

        // User name
        entry.user_name = query_process_username(hProcess);
    }

    return entry;
}

inline result<std::vector<process_list::process_entry>> processes() {
    const ::HANDLE raw_snapshot =
        ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (raw_snapshot == INVALID_HANDLE_VALUE) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                   std::system_category()));
    }
    const handle_guard guard(raw_snapshot);

    std::vector<process_list::process_entry> list;
    ::PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    if (!::Process32FirstW(raw_snapshot, &pe)) {
        const ::DWORD err = ::GetLastError();
        if (err == ERROR_NO_MORE_FILES) {
            return list;
        }
        return fail(
            std::error_code(static_cast<int>(err), std::system_category()));
    }

    for (;;) {
        if (pe.th32ProcessID != 0U) {
            list.push_back(make_entry_from_snapshot(pe));
        }
        if (!::Process32NextW(raw_snapshot, &pe)) {
            const ::DWORD err = ::GetLastError();
            if (err != ERROR_NO_MORE_FILES) {
                return fail(std::error_code(static_cast<int>(err),
                                            std::system_category()));
            }
            break;
        }
    }

    process_list_common::sort_processes(list);
    return list;
}

inline result<std::uint32_t> process_count() {
    const ::HANDLE raw_snapshot =
        ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (raw_snapshot == INVALID_HANDLE_VALUE) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                   std::system_category()));
    }
    const handle_guard guard(raw_snapshot);

    std::uint32_t count = 0U;
    ::PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    if (!::Process32FirstW(raw_snapshot, &pe)) {
        const ::DWORD err = ::GetLastError();
        if (err == ERROR_NO_MORE_FILES) {
            return 0U;
        }
        return fail(
            std::error_code(static_cast<int>(err), std::system_category()));
    }

    for (;;) {
        if (pe.th32ProcessID != 0U) {
            ++count;
        }
        if (!::Process32NextW(raw_snapshot, &pe)) {
            const ::DWORD err = ::GetLastError();
            if (err != ERROR_NO_MORE_FILES) {
                return fail(std::error_code(static_cast<int>(err),
                                            std::system_category()));
            }
            break;
        }
    }

    return count;
}

inline result<process_list::process_entry> find_process(std::uint32_t pid) {
    if (pid == 0U) {
        return fail(errc::not_found);
    }
    const ::HANDLE raw_snapshot =
        ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (raw_snapshot == INVALID_HANDLE_VALUE) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                   std::system_category()));
    }
    const handle_guard guard(raw_snapshot);

    ::PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    if (!::Process32FirstW(raw_snapshot, &pe)) {
        const ::DWORD err = ::GetLastError();
        if (err == ERROR_NO_MORE_FILES) {
            return fail(errc::not_found);
        }
        return fail(
            std::error_code(static_cast<int>(err), std::system_category()));
    }

    for (;;) {
        if (pe.th32ProcessID == pid) {
            return make_entry_from_snapshot(pe);
        }
        if (!::Process32NextW(raw_snapshot, &pe)) {
            const ::DWORD err = ::GetLastError();
            if (err != ERROR_NO_MORE_FILES) {
                return fail(std::error_code(static_cast<int>(err),
                                            std::system_category()));
            }
            break;
        }
    }

    return fail(errc::not_found);
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
        if (process_list_common::matches_process_name(entry, name, true)) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}

} // namespace process_list_backend
} // namespace detail
} // namespace syscape

#if defined(SYSCAPE_DETAIL_PROCESS_LIST_DEFINED_WIN32_WINNT)
#undef _WIN32_WINNT
#undef SYSCAPE_DETAIL_PROCESS_LIST_DEFINED_WIN32_WINNT
#endif

#if defined(SYSCAPE_DETAIL_PROCESS_LIST_DEFINED_WINVER)
#undef WINVER
#undef SYSCAPE_DETAIL_PROCESS_LIST_DEFINED_WINVER
#endif

#endif // SYSCAPE_DETAIL_PROCESS_LIST_WINDOWS_HPP
