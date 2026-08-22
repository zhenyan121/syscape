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

    return failures == 0 ? 0 : 1;
}
