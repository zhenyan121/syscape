#include <iostream>

#include <syscape/memory.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_memory_queries() {
    const auto total = syscape::memory::physical_memory_bytes();
    expect(total && *total > 0, "total physical memory must be positive");

    const auto free_bytes = syscape::memory::available_memory_bytes();
    expect(free_bytes.has_value() ||
               free_bytes.error() == syscape::errc::not_supported,
           "available memory query must succeed or report not_supported");
    if (free_bytes && total) {
        expect(*free_bytes <= *total,
               "available memory must not exceed total physical memory");
    }

    const auto page_size = syscape::memory::page_size_bytes();
    expect(page_size && *page_size > 0 && (*page_size & (*page_size - 1)) == 0,
           "page size must be a positive power of two");

    const auto swap = syscape::memory::swap_status();
    expect(swap.has_value() || swap.error() == syscape::errc::not_supported,
           "swap query must succeed or report not_supported on GNU/Hurd");

    const auto commit = syscape::memory::commit_status();
    expect(commit.error() == syscape::errc::not_supported,
           "commit_status query must report not_supported on GNU/Hurd");

    const auto load = syscape::memory::memory_load_percent();
    expect(load.has_value() || load.error() == syscape::errc::not_supported,
           "memory load query must succeed or report not_supported");
    if (load) {
        expect(*load <= 100, "memory load percent must be <= 100");
    }

    const auto huge_page = syscape::memory::huge_page_size_bytes();
    expect(huge_page.error() == syscape::errc::not_supported,
           "huge page size query must report not_supported on GNU/Hurd");

    const auto huge_pool = syscape::memory::huge_page_pool_status();
    expect(huge_pool.error() == syscape::errc::not_supported,
           "huge page pool status query must report not_supported on GNU/Hurd");
}

} // namespace

int main() {
    test_memory_queries();
    return failures == 0 ? 0 : 1;
}
