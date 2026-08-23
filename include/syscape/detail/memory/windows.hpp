#ifndef SYSCAPE_DETAIL_MEMORY_WINDOWS_HPP
#define SYSCAPE_DETAIL_MEMORY_WINDOWS_HPP

#include <cstdint>
#include <limits>
#include <system_error>
#include <windows.h>

#include <psapi.h>

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> page_size_bytes() {
    ::SYSTEM_INFO information {};
    ::GetSystemInfo(&information);
    if (information.dwPageSize == 0U) { return fail(errc::malformed_data); }
    return static_cast<std::uint64_t>(information.dwPageSize);
}

/// Snapshot of physical memory from GlobalMemoryStatusEx.
struct global_memory_snapshot {
    std::uint64_t total_bytes = 0U;
    std::uint64_t available_bytes = 0U;
    std::uint32_t load_percent = 0U;
};

inline result<global_memory_snapshot> query_global_memory_status() {
    ::MEMORYSTATUSEX status {};
    status.dwLength = sizeof(status);
    if (::GlobalMemoryStatusEx(&status) == FALSE) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    if (status.ullAvailPhys > status.ullTotalPhys ||
        status.ullTotalPhys == 0ULL || status.dwMemoryLoad > 100U) {
        return fail(errc::malformed_data);
    }
    global_memory_snapshot memory;
    memory.total_bytes = static_cast<std::uint64_t>(status.ullTotalPhys);
    memory.available_bytes = static_cast<std::uint64_t>(status.ullAvailPhys);
    memory.load_percent = static_cast<std::uint32_t>(status.dwMemoryLoad);
    return memory;
}

inline result<std::uint64_t> physical_memory_bytes() {
    const result<global_memory_snapshot> memory = query_global_memory_status();
    if (!memory) { return fail(memory.error()); }
    return memory->total_bytes;
}

inline result<std::uint64_t> available_memory_bytes() {
    const result<global_memory_snapshot> memory = query_global_memory_status();
    if (!memory) { return fail(memory.error()); }
    return memory->available_bytes;
}

/// Converts the page counts returned by GetPerformanceInfo into bytes.
///
/// CommitTotal and CommitLimit describe one system-wide snapshot. The
/// conversion is kept separate from the native call so zero-page-size and
/// multiplication-overflow paths remain structurally testable.
inline result<memory_common::commit_usage> interpret_commit_information(
    std::uint64_t committed_pages,
    std::uint64_t commit_limit_pages,
    std::uint64_t page_size_bytes) {
    if (page_size_bytes == 0U || commit_limit_pages == 0U) {
        return fail(errc::malformed_data);
    }
    const std::uint64_t maximum =
        (std::numeric_limits<std::uint64_t>::max)();
    if (committed_pages > maximum / page_size_bytes ||
        commit_limit_pages > maximum / page_size_bytes) {
        return fail(errc::value_too_large);
    }
    memory_common::commit_usage usage;
    usage.committed_bytes = committed_pages * page_size_bytes;
    usage.commit_limit_bytes = commit_limit_pages * page_size_bytes;
    return usage;
}

inline result<memory_common::commit_usage> commit_status() {
    ::PERFORMANCE_INFORMATION information {};
    information.cb = static_cast<::DWORD>(sizeof(information));
    if (::GetPerformanceInfo(&information,
                             static_cast<::DWORD>(sizeof(information))) ==
        FALSE) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    return interpret_commit_information(
        static_cast<std::uint64_t>(information.CommitTotal),
        static_cast<std::uint64_t>(information.CommitLimit),
        static_cast<std::uint64_t>(information.PageSize));
}

/// Reports the documented approximate percentage of physical memory in use.
inline result<std::uint32_t> memory_load_percent() {
    const result<global_memory_snapshot> memory = query_global_memory_status();
    if (!memory) { return fail(memory.error()); }
    return memory->load_percent;
}

/// Reports the documented minimum large-page size in bytes.
///
/// A zero return means the running system exposes no large-page minimum, so
/// the capability is unsupported rather than malformed.
inline result<std::uint64_t> huge_page_size_bytes() {
    const ::SIZE_T minimum = ::GetLargePageMinimum();
    if (minimum == 0U) { return fail(errc::not_supported); }
    return static_cast<std::uint64_t>(minimum);
}

/// Returns not_supported because Windows exposes no huge-page pool counts
/// through an acceptable public source.
inline result<memory_common::huge_page_pool_usage> huge_page_pool_status() {
    return fail(errc::not_supported);
}

/// Returns not_supported because Windows exposes no pressure-stall
/// accounting through an acceptable public source.
inline result<memory_common::pressure_status> memory_pressure() {
    return fail(errc::not_supported);
}

/// Returns not_supported because Windows exposes no paging-space capacity
/// through an acceptable public source.
///
/// GlobalMemoryStatusEx reports the commit limit and remaining commit
/// charge, which cover RAM plus paging files and are scoped to whichever is
/// smaller, the system or the current process (job limits included). Those
/// fields cannot express system paging-space capacity and never report zero
/// when no pagefile exists, so they would misrepresent this query's contract.
inline result<memory_common::swap_usage> swap_status() {
    return fail(errc::not_supported);
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#endif
