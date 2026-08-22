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

    return failures == 0 ? 0 : 1;
}
