#include <cstdint>
#include <iostream>
#include <system_error>

#include <syscape/memory.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

} // namespace

int main() {
    const auto page_size = syscape::memory::page_size_bytes();
    expect(page_size && *page_size > 0U &&
                (*page_size & (*page_size - 1U)) == 0U,
           "macOS must report a positive power-of-two page size");

    const auto physical = syscape::memory::physical_memory_bytes();
    const auto available = syscape::memory::available_memory_bytes();
    expect(physical && available && *available <= *physical,
           "macOS must report positive physical memory from hw.memsize and "
           "free plus inactive memory must not exceed it");

    const auto swap = syscape::memory::swap_status();
    if (swap) {
        expect(swap->free_bytes <= swap->total_bytes,
               "Unused swap capacity must never exceed total capacity");
    }

    const auto load = syscape::memory::memory_load_percent();
    expect(load && *load <= 100U,
           "macOS must estimate utilization within the percentage range");
    if (load && physical && available) {
        // The estimate must track the same free-plus-inactive definition
        // that backs available_memory_bytes(), with small drift allowed
        // between the two sampling instants.
        const std::uint64_t used = *physical - *available;
        const std::uint64_t recomputed =
            (used * 100U + *physical / 2U) / *physical;
        const std::uint64_t difference =
            static_cast<std::uint64_t>(*load) > recomputed
                ? static_cast<std::uint64_t>(*load) - recomputed
                : recomputed - static_cast<std::uint64_t>(*load);
        expect(difference <= 2U,
               "The load estimate must track the availability definition");
    }

    const auto commit = syscape::memory::commit_status();
    expect(!commit && commit.error() == syscape::errc::not_supported,
           "Darwin exposes no acceptable commit-accounting source");

    const auto huge_size = syscape::memory::huge_page_size_bytes();
    expect(!huge_size && huge_size.error() == syscape::errc::not_supported,
           "Darwin exposes no acceptable huge-page size source");

    const auto pool = syscape::memory::huge_page_pool_status();
    expect(!pool && pool.error() == syscape::errc::not_supported,
           "Darwin exposes no acceptable huge-page pool source");

    const auto pressure = syscape::memory::memory_pressure();
    expect(!pressure && pressure.error() == syscape::errc::not_supported,
           "Darwin exposes no acceptable pressure-stall source");

    return failures == 0 ? 0 : 1;
}
