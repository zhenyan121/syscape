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
    const auto phys = syscape::memory::physical_memory_bytes();
    expect(phys.has_value() && *phys == 65536U,
           "physical memory must report total heap size on T-Kernel");

    const auto avail = syscape::memory::available_memory_bytes();
    expect(avail.has_value() && *avail == 32768U,
           "available memory must report free memory pool size on T-Kernel");

    const auto load = syscape::memory::memory_load_percent();
    expect(load.has_value() && *load == 50U,
           "memory load percent must report 50% on T-Kernel");

    const auto page = syscape::memory::page_size_bytes();
    expect(!page && page.error() == syscape::errc::not_supported,
           "page size must report not_supported on T-Kernel");

    const auto swap = syscape::memory::swap_status();
    expect(!swap && swap.error() == syscape::errc::not_supported,
           "swap status must report not_supported on T-Kernel");

    const auto commit = syscape::memory::commit_status();
    expect(!commit && commit.error() == syscape::errc::not_supported,
           "commit status must report not_supported on T-Kernel");

    const auto huge_size = syscape::memory::huge_page_size_bytes();
    expect(!huge_size && huge_size.error() == syscape::errc::not_supported,
           "huge page size must report not_supported on T-Kernel");

    const auto huge_pool = syscape::memory::huge_page_pool_status();
    expect(!huge_pool && huge_pool.error() == syscape::errc::not_supported,
           "huge page pool must report not_supported on T-Kernel");

    const auto press = syscape::memory::memory_pressure();
    expect(!press && press.error() == syscape::errc::not_supported,
           "memory pressure must report not_supported on T-Kernel");
}

} // namespace

int main() {
    test_memory_queries();
    return failures == 0 ? 0 : 1;
}
