#include <iostream>

#include <syscape/memory.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

void test_memory_queries() {
    const auto page = syscape::memory::page_size_bytes();
    if (page.has_value()) {
        expect(*page > 0, "page size must be positive");
    } else {
        expect(page.error() == syscape::errc::not_supported,
               "page size failure must report not_supported");
    }

    const auto phys = syscape::memory::physical_memory_bytes();
#if defined(SYSCAPE_LYNXOS_TOTAL_HEAP_SIZE)
    expect(phys.has_value() && *phys == SYSCAPE_LYNXOS_TOTAL_HEAP_SIZE,
           "physical memory must match SYSCAPE_LYNXOS_TOTAL_HEAP_SIZE");
#elif defined(LYNXOS_TOTAL_HEAP_SIZE)
    expect(phys.has_value() && *phys == LYNXOS_TOTAL_HEAP_SIZE,
           "physical memory must match LYNXOS_TOTAL_HEAP_SIZE");
#else
    expect(!phys && phys.error() == syscape::errc::not_supported,
           "physical memory must report not_supported on LynxOS by default");
#endif

    const auto avail = syscape::memory::available_memory_bytes();
#if defined(SYSCAPE_LYNXOS_FREE_HEAP_SIZE)
    expect(avail.has_value() && *avail == SYSCAPE_LYNXOS_FREE_HEAP_SIZE,
           "available memory must match SYSCAPE_LYNXOS_FREE_HEAP_SIZE");
#elif defined(LYNXOS_FREE_HEAP_SIZE)
    expect(avail.has_value() && *avail == LYNXOS_FREE_HEAP_SIZE,
           "available memory must match LYNXOS_FREE_HEAP_SIZE");
#else
    expect(!avail && avail.error() == syscape::errc::not_supported,
           "available memory must report not_supported on LynxOS by default");
#endif

    const auto load = syscape::memory::memory_load_percent();
#if (defined(SYSCAPE_LYNXOS_TOTAL_HEAP_SIZE) ||                                \
     defined(LYNXOS_TOTAL_HEAP_SIZE)) &&                                       \
    (defined(SYSCAPE_LYNXOS_FREE_HEAP_SIZE) || defined(LYNXOS_FREE_HEAP_SIZE))
    expect(load.has_value() && *load <= 100,
           "memory load percent must succeed and be <= 100");
#else
    expect(!load, "memory load percent must report not_supported by default");
#endif

    const auto swap = syscape::memory::swap_status();
    expect(!swap && swap.error() == syscape::errc::not_supported,
           "swap status must report not_supported on LynxOS");

    const auto commit = syscape::memory::commit_status();
    expect(!commit && commit.error() == syscape::errc::not_supported,
           "commit status must report not_supported on LynxOS");

    const auto huge_size = syscape::memory::huge_page_size_bytes();
    expect(!huge_size && huge_size.error() == syscape::errc::not_supported,
           "huge page size must report not_supported on LynxOS");

    const auto huge_pool = syscape::memory::huge_page_pool_status();
    expect(!huge_pool && huge_pool.error() == syscape::errc::not_supported,
           "huge page pool status must report not_supported on LynxOS");

    const auto pressure = syscape::memory::memory_pressure();
    expect(!pressure && pressure.error() == syscape::errc::not_supported,
           "memory pressure must report not_supported on LynxOS");
}

} // namespace

int main() {
    test_memory_queries();
    return failures == 0 ? 0 : 1;
}
