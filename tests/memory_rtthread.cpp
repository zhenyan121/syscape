#include <iostream>

#include "rtdef.h"
#include "rtthread.h"

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
#if defined(RT_NO_HEAP)
    const auto phys = syscape::memory::physical_memory_bytes();
    expect(!phys && phys.error() == syscape::errc::not_supported,
           "physical memory must report not_supported when RT_USING_HEAP is "
           "disabled");

    const auto avail = syscape::memory::available_memory_bytes();
    expect(!avail && avail.error() == syscape::errc::not_supported,
           "available memory must report not_supported when RT_USING_HEAP is "
           "disabled");

    const auto load = syscape::memory::memory_load_percent();
    expect(
        !load && load.error() == syscape::errc::not_supported,
        "memory load percent must report not_supported when RT_USING_HEAP is "
        "disabled");
#else
    const auto phys = syscape::memory::physical_memory_bytes();
    expect(phys.has_value() && *phys == 65536ULL,
           "physical memory must report total heap from rt_memory_info");

    const auto avail = syscape::memory::available_memory_bytes();
    expect(avail.has_value() && *avail == (65536ULL - 16384ULL),
           "available memory must report free heap from rt_memory_info");

    const auto load = syscape::memory::memory_load_percent();
    expect(load.has_value() && *load == 25U,
           "memory load percent must report 25% for 16384/65536");

    rtthread_mock_set_memory(100000, 50000, 60000);
    const auto phys2 = syscape::memory::physical_memory_bytes();
    expect(phys2.has_value() && *phys2 == 100000ULL,
           "physical memory must dynamically reflect updated total");
    const auto avail2 = syscape::memory::available_memory_bytes();
    expect(avail2.has_value() && *avail2 == 50000ULL,
           "available memory must dynamically reflect updated available");
    const auto load2 = syscape::memory::memory_load_percent();
    expect(load2.has_value() && *load2 == 50U,
           "memory load percent must dynamically reflect 50%");

    // Test malformed data when used > total
    rtthread_mock_set_memory(50000, 60000, 60000);
    const auto avail_err = syscape::memory::available_memory_bytes();
    expect(!avail_err && avail_err.error() == syscape::errc::malformed_data,
           "available memory must report malformed_data when used > total");
    const auto load_err = syscape::memory::memory_load_percent();
    expect(!load_err && load_err.error() == syscape::errc::malformed_data,
           "memory load percent must report malformed_data when used > total");

    rtthread_mock_reset();
#endif

    const auto page = syscape::memory::page_size_bytes();
    expect(!page && page.error() == syscape::errc::not_supported,
           "page size must report not_supported on RT-Thread");

    const auto swap = syscape::memory::swap_status();
    expect(!swap && swap.error() == syscape::errc::not_supported,
           "swap status must report not_supported on RT-Thread");

    const auto commit = syscape::memory::commit_status();
    expect(!commit && commit.error() == syscape::errc::not_supported,
           "commit status must report not_supported on RT-Thread");

    const auto huge_size = syscape::memory::huge_page_size_bytes();
    expect(!huge_size && huge_size.error() == syscape::errc::not_supported,
           "huge page size must report not_supported on RT-Thread");

    const auto huge_pool = syscape::memory::huge_page_pool_status();
    expect(!huge_pool && huge_pool.error() == syscape::errc::not_supported,
           "huge page pool must report not_supported on RT-Thread");

    const auto pressure = syscape::memory::memory_pressure();
    expect(!pressure && pressure.error() == syscape::errc::not_supported,
           "memory pressure must report not_supported on RT-Thread");
}

} // namespace

int main() {
    test_memory_queries();
    return failures == 0 ? 0 : 1;
}
