#ifndef SYSCAPE_DETAIL_MEMORY_COMMON_HPP
#define SYSCAPE_DETAIL_MEMORY_COMMON_HPP

#include <cstdint>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_common {

/// Swap or pagefile capacity values shared by the memory backends.
///
/// Both fields are byte counts. Zero totals are valid data that mean the
/// platform has no configured paging space; they are not error sentinels.
struct swap_usage {
    /// Configured swap or pagefile capacity in bytes.
    std::uint64_t total_bytes = 0U;
    /// Unused swap or pagefile capacity in bytes.
    std::uint64_t free_bytes = 0U;
};

/// Rejects a swap snapshot whose unused capacity exceeds its total.
inline result<swap_usage> validate_swap_usage(result<swap_usage> value) {
    if (!value) { return fail(value.error()); }
    if (value->free_bytes > value->total_bytes) {
        return fail(errc::malformed_data);
    }
    return value;
}

} // namespace memory_common
} // namespace detail
} // namespace syscape

#endif
