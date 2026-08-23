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
           "Windows must report a positive power-of-two page size");

    const auto physical = syscape::memory::physical_memory_bytes();
    const auto available = syscape::memory::available_memory_bytes();
    expect(physical && available && *available <= *physical,
           "Windows must report positive physical memory and available "
           "memory must not exceed it");

    const auto swap = syscape::memory::swap_status();
    expect(!swap && swap.error() == syscape::errc::not_supported,
           "Windows commit accounting must not masquerade as paging-space "
           "capacity");

    const auto commit = syscape::memory::commit_status();
    if (commit) {
        // The documented limit covers RAM plus paging files scoped to
        // whichever is smaller, the system or the current process; a
        // heuristic-overcommit caller can legitimately exceed it.
        expect(commit->commit_limit_bytes > 0U,
               "The effective commit limit must be positive");
    } else {
        expect(commit.error().category() == std::system_category(),
               "Commit failures must preserve native system errors");
    }

    const auto load = syscape::memory::memory_load_percent();
    expect(load && *load <= 100U,
           "Windows must report its documented load percentage within the "
           "percentage range");

    const auto huge_size = syscape::memory::huge_page_size_bytes();
    if (huge_size) {
        expect(*huge_size > 0U && (*huge_size & (*huge_size - 1U)) == 0U,
               "A reported large-page minimum must be a positive power of "
               "two");
        if (page_size) {
            expect(*huge_size >= *page_size,
                   "Large pages are never smaller than base pages");
        }
    } else {
        expect(huge_size.error() == syscape::errc::not_supported,
               "An absent large-page minimum must be not_supported");
    }

    const auto pool = syscape::memory::huge_page_pool_status();
    expect(!pool && pool.error() == syscape::errc::not_supported,
           "Windows exposes no acceptable huge-page pool source");

    const auto pressure = syscape::memory::memory_pressure();
    expect(!pressure && pressure.error() == syscape::errc::not_supported,
           "Windows exposes no acceptable pressure-stall source");

    return failures == 0 ? 0 : 1;
}
