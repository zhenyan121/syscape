#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <syscape/filesystem.hpp>
#include <syscape/storage.hpp>

namespace {

const char* scheme_name(syscape::storage::partition_scheme scheme) {
    switch (scheme) {
    case syscape::storage::partition_scheme::gpt: return "GPT";
    case syscape::storage::partition_scheme::mbr: return "MBR";
    case syscape::storage::partition_scheme::apple: return "Apple (APM)";
    case syscape::storage::partition_scheme::raw: return "Raw / Unpartitioned";
    case syscape::storage::partition_scheme::unknown: return "Unknown";
    }
    return "Unknown";
}

} // namespace

int main() {
    std::cout << "=== Syscape Filesystem & Partition Storage Example ===" << std::endl;

    // Mounted Filesystems
    std::cout << "\n[Mounted Filesystems & Volume Capacity]" << std::endl;
    if (const auto mounts = syscape::filesystem::mounts()) {
        std::cout << "  Total Mount Entries: " << mounts->size() << std::endl;
        for (const auto& m : *mounts) {
            // Query capacity snapshot for each mount point
            const auto space_res = syscape::filesystem::space(m.mount_point);
            std::cout << "  Mount: " << m.mount_point
                      << " (" << m.file_system_type
                      << (m.source.empty() ? "" : ", from " + m.source)
                      << ")" << std::endl;

            if (space_res) {
                const auto cap_gib = space_res->capacity_bytes / (1024ULL * 1024ULL * 1024ULL);
                const auto avail_gib = space_res->available_bytes / (1024ULL * 1024ULL * 1024ULL);
                std::cout << "    Capacity:   " << avail_gib << " GiB available of "
                          << cap_gib << " GiB (" << space_res->capacity_bytes << " bytes)" << std::endl;
                std::cout << "    Block Size: " << space_res->block_size_bytes << " bytes, "
                          << (space_res->read_only ? "Read-Only" : "Read-Write") << std::endl;
            }
        }
    }

    // Path Limits
    std::cout << "\n[Filesystem Path Length Limits]" << std::endl;
    if (const auto comp_lim = syscape::filesystem::max_component_length("/")) {
        std::cout << "  Root (/) Max Component Name: "
                  << (comp_lim->indeterminate ? "Indeterminate" : std::to_string(comp_lim->length) + " bytes")
                  << std::endl;
    }

    // Physical Storage Partitions
    std::cout << "\n[Disk Partitions & Layout Tables]" << std::endl;
    if (const auto parts = syscape::storage::partitions()) {
        std::cout << "  Total Partitions: " << parts->size() << std::endl;
        for (const auto& p : *parts) {
            std::cout << "  Partition [" << p.identifier << "] on "
                      << p.disk_identifier
                      << " (Part #" << p.partition_number
                      << ", " << scheme_name(p.scheme) << "):" << std::endl;
            if (p.has_size_bytes) {
                std::cout << "    Size:       "
                          << (p.size_bytes / (1024ULL * 1024ULL * 1024ULL)) << " GiB ("
                          << p.size_bytes << " bytes)" << std::endl;
            }
            if (p.has_start_offset_bytes) {
                std::cout << "    Start:      " << p.start_offset_bytes << " bytes" << std::endl;
            }
            if (p.is_mounted) {
                std::cout << "    Mount Point:" << p.mount_point << std::endl;
            }
        }
    }

    return 0;
}
