#ifndef SYSCAPE_DETAIL_MEMORY_WINDOWS_HPP
#define SYSCAPE_DETAIL_MEMORY_WINDOWS_HPP

#include <cstdint>
#include <system_error>
#include <windows.h>

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
struct physical_memory_status {
    std::uint64_t total_bytes = 0U;
    std::uint64_t available_bytes = 0U;
};

inline result<physical_memory_status> query_physical_memory() {
    ::MEMORYSTATUSEX status {};
    status.dwLength = sizeof(status);
    if (::GlobalMemoryStatusEx(&status) == FALSE) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    if (status.ullAvailPhys > status.ullTotalPhys ||
        status.ullTotalPhys == 0ULL) {
        return fail(errc::malformed_data);
    }
    physical_memory_status memory;
    memory.total_bytes = static_cast<std::uint64_t>(status.ullTotalPhys);
    memory.available_bytes = static_cast<std::uint64_t>(status.ullAvailPhys);
    return memory;
}

inline result<std::uint64_t> physical_memory_bytes() {
    const result<physical_memory_status> memory = query_physical_memory();
    if (!memory) { return fail(memory.error()); }
    return memory->total_bytes;
}

inline result<std::uint64_t> available_memory_bytes() {
    const result<physical_memory_status> memory = query_physical_memory();
    if (!memory) { return fail(memory.error()); }
    return memory->available_bytes;
}

/// Returns not_supported because Windows exposes no paging-space capacity
/// through an acceptable public source.
///
/// GlobalMemoryStatusEx reports the commit limit and remaining commit charge,
/// which cover RAM plus paging files and are scoped to whichever is smaller,
/// the system or the current process (job limits included). Those fields
/// cannot express system paging-space capacity and never report zero when no
/// pagefile exists, so they would misrepresent this query's contract.
inline result<memory_common::swap_usage> swap_status() {
    return fail(errc::not_supported);
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#endif
