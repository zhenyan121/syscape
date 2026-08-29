#ifndef SYSCAPE_DETAIL_NUMA_COMMON_HPP
#define SYSCAPE_DETAIL_NUMA_COMMON_HPP

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <syscape/result.hpp>

namespace syscape {
namespace numa {

/// Represents a single Non-Uniform Memory Access (NUMA) node.
struct numa_node {
    /// Zero-based NUMA node identifier.
    std::uint32_t id = 0U;

    /// List of online logical processor IDs associated with this node, in ascending order.
    /// Memory-only nodes without attached CPUs will have an empty collection.
    std::vector<std::uint32_t> logical_processors;

    /// Total physical memory capacity attached to this node in bytes.
    /// std::nullopt if the node is CPU-only or the platform does not report per-node capacity.
    std::optional<std::uint64_t> total_memory_bytes;

    /// Free physical memory available on this node in bytes.
    /// std::nullopt if the platform does not report per-node free memory.
    std::optional<std::uint64_t> free_memory_bytes;

    /// Used physical memory on this node in bytes.
    /// std::nullopt if the platform does not report per-node used memory.
    std::optional<std::uint64_t> used_memory_bytes;

    /// Relative distance / latency to each NUMA node in the system.
    /// Index i represents the distance to NUMA node i.
    /// Local distance to self is typically 10 on ACPI SLIT systems.
    /// Empty if the platform does not expose inter-node distances.
    std::vector<std::uint32_t> distances;

    /// Indicates whether this node is currently online.
    bool is_online = true;
};

} // namespace numa

namespace detail {
namespace numa_common {

using numa_node = ::syscape::numa::numa_node;

/// Validates that a NUMA node's fields satisfy internal invariants.
inline result<numa_node> validate_numa_node(numa_node node) {
    if (node.total_memory_bytes && node.free_memory_bytes) {
        if (*node.free_memory_bytes > *node.total_memory_bytes) {
            return fail(errc::malformed_data);
        }
    }
    if (node.total_memory_bytes && node.used_memory_bytes) {
        if (*node.used_memory_bytes > *node.total_memory_bytes) {
            return fail(errc::malformed_data);
        }
    }
    if (!std::is_sorted(node.logical_processors.begin(),
                        node.logical_processors.end())) {
        std::sort(node.logical_processors.begin(),
                  node.logical_processors.end());
    }
    node.logical_processors.erase(
        std::unique(node.logical_processors.begin(),
                    node.logical_processors.end()),
        node.logical_processors.end());

    return node;
}

/// Validates a collection of NUMA nodes.
inline result<std::vector<numa_node>> validate_numa_nodes(
    std::vector<numa_node> nodes) {
    for (auto& n : nodes) {
        const auto validated = validate_numa_node(std::move(n));
        if (!validated) { return fail(validated.error()); }
        n = *validated;
    }
    std::sort(nodes.begin(), nodes.end(),
              [](const numa_node& a, const numa_node& b) noexcept {
                  return a.id < b.id;
              });
    return nodes;
}

} // namespace numa_common
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_NUMA_COMMON_HPP
