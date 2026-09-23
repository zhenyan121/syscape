#include <iostream>

#include "cpu.h"
#include "kernel_defines.h"
#include "malloc_monitor.h"

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
           "physical memory must report RAM size from cpu_get_ram_size on RIOT "
           "OS");

    const auto avail = syscape::memory::available_memory_bytes();
    expect(
        avail.has_value() && *avail == 32768U,
        "available memory must report free heap from get_mem_usage on RIOT OS");

    const auto load = syscape::memory::memory_load_percent();
    expect(load.has_value() && *load == 50U,
           "memory load percent must report 50% on initial state");

    // Dynamic runtime update of available memory
    riot_mock_set_available_memory(16384U);
    const auto avail2 = syscape::memory::available_memory_bytes();
    expect(avail2.has_value() && *avail2 == 16384U,
           "available memory must dynamically reflect get_mem_usage updates");
    const auto load2 = syscape::memory::memory_load_percent();
    expect(load2.has_value() && *load2 == 75U,
           "memory load percent must dynamically compute 75% load");

    // Dynamic runtime update of physical RAM size
    riot_mock_set_ram_size(131072U);
    riot_mock_set_available_memory(65536U);
    const auto phys3 = syscape::memory::physical_memory_bytes();
    expect(phys3.has_value() && *phys3 == 131072U,
           "physical memory must dynamically reflect cpu_get_ram_size updates");
    const auto load3 = syscape::memory::memory_load_percent();
    expect(load3.has_value() && *load3 == 50U,
           "memory load percent must recompute with updated RAM size");

    // Error handling: available > physical memory
    riot_mock_set_ram_size(65536U);
    riot_mock_set_available_memory(100000U);
    const auto load_invalid = syscape::memory::memory_load_percent();
    expect(!load_invalid &&
               load_invalid.error() == syscape::errc::malformed_data,
           "memory load percent must report malformed_data when available > "
           "physical");

    // Error handling: RAM size is 0
    riot_mock_set_ram_size(0U);
    const auto phys_zero = syscape::memory::physical_memory_bytes();
    expect(!phys_zero && phys_zero.error() == syscape::errc::malformed_data,
           "physical memory must report malformed_data when RAM size is 0");

    // Restore clean mock state
    riot_mock_set_ram_size(65536U);
    riot_mock_set_available_memory(32768U);

    const auto page = syscape::memory::page_size_bytes();
    expect(!page && page.error() == syscape::errc::not_supported,
           "page size must report not_supported on RIOT OS");

    const auto swap = syscape::memory::swap_status();
    expect(!swap && swap.error() == syscape::errc::not_supported,
           "swap status must report not_supported on RIOT OS");

    const auto commit = syscape::memory::commit_status();
    expect(!commit && commit.error() == syscape::errc::not_supported,
           "commit status must report not_supported on RIOT OS");

    const auto huge_size = syscape::memory::huge_page_size_bytes();
    expect(!huge_size && huge_size.error() == syscape::errc::not_supported,
           "huge page size must report not_supported on RIOT OS");

    const auto huge_pool = syscape::memory::huge_page_pool_status();
    expect(!huge_pool && huge_pool.error() == syscape::errc::not_supported,
           "huge page pool must report not_supported on RIOT OS");

    const auto press = syscape::memory::memory_pressure();
    expect(!press && press.error() == syscape::errc::not_supported,
           "memory pressure must report not_supported on RIOT OS");
}

} // namespace

int main() {
    test_memory_queries();
    return failures == 0 ? 0 : 1;
}
