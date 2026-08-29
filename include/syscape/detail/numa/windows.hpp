#ifndef SYSCAPE_DETAIL_NUMA_WINDOWS_HPP
#define SYSCAPE_DETAIL_NUMA_WINDOWS_HPP

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0601
#error "syscape/numa.hpp requires _WIN32_WINNT >= 0x0601 on Windows"
#endif

#if defined(WINVER) && WINVER < 0x0601
#error "syscape/numa.hpp requires WINVER >= 0x0601 on Windows"
#endif

#if !defined(_WIN32_WINNT)
#define SYSCAPE_DETAIL_NUMA_DEFINED_WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#if !defined(WINVER)
#define SYSCAPE_DETAIL_NUMA_DEFINED_WINVER
#define WINVER 0x0601
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <system_error>
#include <utility>
#include <vector>

#include <syscape/detail/numa/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace numa_backend {

typedef BOOL(WINAPI* PFN_GetNumaNodeProcessorMask2)(
    USHORT NodeNumber,
    PGROUP_AFFINITY ProcessorMasks,
    USHORT ProcessorMaskCount,
    PUSHORT RequiredMaskCount);

inline PFN_GetNumaNodeProcessorMask2 get_numa_node_processor_mask2_ptr() noexcept {
    const HMODULE kernel32 = ::GetModuleHandleW(L"kernel32.dll");
    if (kernel32 == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<PFN_GetNumaNodeProcessorMask2>(
        reinterpret_cast<void*>(::GetProcAddress(kernel32, "GetNumaNodeProcessorMask2")));
}

inline result<std::vector<GROUP_AFFINITY>> query_node_processor_masks(USHORT node_id) {
    const PFN_GetNumaNodeProcessorMask2 pfn_mask2 = get_numa_node_processor_mask2_ptr();
    if (pfn_mask2 != nullptr) {
        USHORT required_count = 0;
        std::vector<GROUP_AFFINITY> masks;
        for (std::size_t attempt = 0U; attempt < 8U; ++attempt) {
            if (pfn_mask2(node_id, masks.empty() ? nullptr : masks.data(),
                          static_cast<USHORT>(masks.size()), &required_count)) {
                masks.resize(required_count);
                return masks;
            }
            const DWORD err = ::GetLastError();
            if (err == ERROR_INVALID_PARAMETER) {
                return fail(errc::not_found);
            }
            if (err != ERROR_INSUFFICIENT_BUFFER) {
                return fail(std::error_code(static_cast<int>(err), std::system_category()));
            }
            if (required_count == 0) {
                return std::vector<GROUP_AFFINITY>{};
            }
            masks.resize(required_count);
        }
        return fail(errc::temporarily_unavailable);
    }

    GROUP_AFFINITY single_mask{};
    if (!::GetNumaNodeProcessorMaskEx(node_id, &single_mask)) {
        const DWORD err = ::GetLastError();
        if (err == ERROR_INVALID_PARAMETER) {
            return fail(errc::not_found);
        }
        return fail(std::error_code(static_cast<int>(err), std::system_category()));
    }
    return std::vector<GROUP_AFFINITY>{single_mask};
}

inline result<::syscape::numa::numa_node> read_single_node(std::uint32_t node_id) {
    ULONG highest_node = 0;
    if (!::GetNumaHighestNodeNumber(&highest_node)) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    if (node_id > highest_node) {
        return fail(errc::not_found);
    }

    const auto masks = query_node_processor_masks(static_cast<USHORT>(node_id));
    if (!masks) {
        return fail(masks.error());
    }

    ::syscape::numa::numa_node node;
    node.id = node_id;
    node.is_online = true;

    constexpr std::size_t kaffinity_bits =
        static_cast<std::size_t>(std::numeric_limits<KAFFINITY>::digits);

    for (const auto& affinity : *masks) {
        for (std::size_t bit = 0U; bit < kaffinity_bits; ++bit) {
            if ((affinity.Mask & (static_cast<KAFFINITY>(1U) << bit)) != 0) {
                node.logical_processors.push_back(
                    static_cast<std::uint32_t>(affinity.Group * kaffinity_bits + bit));
            }
        }
    }

    ULONGLONG available_bytes = 0;
    if (::GetNumaAvailableMemoryNodeEx(static_cast<USHORT>(node_id), &available_bytes)) {
        node.free_memory_bytes = static_cast<std::uint64_t>(available_bytes);
    } else {
        const DWORD err = ::GetLastError();
        if (err != ERROR_NOT_SUPPORTED && err != ERROR_INVALID_PARAMETER) {
            return fail(std::error_code(static_cast<int>(err), std::system_category()));
        }
    }

    // Windows Win32 does not provide an inter-node SLIT distance matrix.
    // distances remains empty per public contract.

    return numa_common::validate_numa_node(std::move(node));
}

inline result<std::vector<::syscape::numa::numa_node>> nodes() {
    ULONG highest_node = 0;
    if (!::GetNumaHighestNodeNumber(&highest_node)) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }

    std::vector<::syscape::numa::numa_node> result_nodes;
    for (ULONG id = 0; id <= highest_node; ++id) {
        auto n = read_single_node(static_cast<std::uint32_t>(id));
        if (!n) {
            if (n.error() == errc::not_found) {
                // Gap in node numbers: skip non-existent node
                continue;
            }
            return fail(n.error());
        }
        result_nodes.push_back(std::move(*n));
    }
    if (result_nodes.empty()) {
        return fail(errc::not_found);
    }
    return numa_common::validate_numa_nodes(std::move(result_nodes));
}

inline result<bool> is_numa_available() {
    const auto all_nodes = nodes();
    if (!all_nodes) { return fail(all_nodes.error()); }
    return all_nodes->size() > 1U;
}

inline result<std::uint32_t> node_count() {
    const auto all_nodes = nodes();
    if (!all_nodes) { return fail(all_nodes.error()); }
    return static_cast<std::uint32_t>(all_nodes->size());
}

inline result<::syscape::numa::numa_node> node(std::uint32_t id) {
    return read_single_node(id);
}

inline result<std::uint32_t> current_thread_node() {
    PROCESSOR_NUMBER proc_num{};
    ::GetCurrentProcessorNumberEx(&proc_num);
    USHORT node_num = 0;
    if (!::GetNumaProcessorNodeEx(&proc_num, &node_num)) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    if (node_num == MAXUSHORT || node_num == static_cast<USHORT>(0xFFFF)) {
        return fail(errc::not_found);
    }
    return static_cast<std::uint32_t>(node_num);
}

} // namespace numa_backend
} // namespace detail
} // namespace syscape

#if defined(SYSCAPE_DETAIL_NUMA_DEFINED_WINVER)
#undef WINVER
#undef SYSCAPE_DETAIL_NUMA_DEFINED_WINVER
#endif

#if defined(SYSCAPE_DETAIL_NUMA_DEFINED_WIN32_WINNT)
#undef _WIN32_WINNT
#undef SYSCAPE_DETAIL_NUMA_DEFINED_WIN32_WINNT
#endif

#endif // SYSCAPE_DETAIL_NUMA_WINDOWS_HPP
