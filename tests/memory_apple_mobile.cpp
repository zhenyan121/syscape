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

void test_memory_queries() {
    const auto page = syscape::memory::page_size_bytes();
    expect(page && *page > 0U, "page size must be positive");

    const auto physical = syscape::memory::physical_memory_bytes();
    expect(physical && *physical > 0U,
           "physical memory bytes must be positive");

    const auto available = syscape::memory::available_memory_bytes();
    expect(available.has_value(), "available memory query must succeed");
    if (available && physical) {
        expect(*available <= *physical,
               "available memory must not exceed physical memory");
    }

    const auto swap = syscape::memory::swap_status();
    expect(swap.has_value() || swap.error() == syscape::errc::not_supported ||
               swap.error() == syscape::errc::permission_denied ||
               swap.error() == std::errc::permission_denied ||
               swap.error() == std::errc::operation_not_permitted,
           "swap query must succeed or report expected error");

    const auto load = syscape::memory::memory_load_percent();
    expect(load.has_value() || load.error() == syscape::errc::not_supported,
           "memory load percent query must succeed or report not_supported");
    if (load) {
        expect(*load <= 100U, "load percent must not exceed 100");
    }

    const auto commit = syscape::memory::commit_status();
    expect(commit.has_value() || commit.error() == syscape::errc::not_supported,
           "commit status query must succeed or report not_supported");

    const auto huge_page = syscape::memory::huge_page_size_bytes();
    expect(huge_page.has_value() ||
               huge_page.error() == syscape::errc::not_supported,
           "huge page size query must succeed or report not_supported");

    const auto huge_pool = syscape::memory::huge_page_pool_status();
    expect(huge_pool.has_value() ||
               huge_pool.error() == syscape::errc::not_supported,
           "huge page pool query must succeed or report not_supported");

    const auto pressure = syscape::memory::memory_pressure();
    expect(pressure.has_value() ||
               pressure.error() == syscape::errc::not_supported,
           "memory pressure query must succeed or report not_supported");
}

} // namespace

int main() {
    test_memory_queries();
    return failures == 0 ? 0 : 1;
}
