#include <iostream>

#include "mbed.h"

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
           "physical memory must report total heap size on Mbed OS");

    const auto avail = syscape::memory::available_memory_bytes();
    expect(avail.has_value() && *avail == 49152U,
           "available memory must report free heap space on Mbed OS");

    const auto load = syscape::memory::memory_load_percent();
    expect(load.has_value() && *load == 25U,
           "memory load percent must report 25% on Mbed OS");

    mbed_mock_set_heap_stats(70000U, 65536U);
    const auto invalid_load = syscape::memory::memory_load_percent();
    expect(
        !invalid_load && invalid_load.error() == syscape::errc::malformed_data,
        "memory load percent must reject allocated heap exceeding total heap");
    mbed_mock_set_heap_stats(16384U, 65536U);

    mbed_mock_set_heap_stats(0U, 0U);
    const auto no_avail = syscape::memory::available_memory_bytes();
    expect(!no_avail && no_avail.error() == syscape::errc::not_supported,
           "available memory must report not_supported when heap stats "
           "reserved_size is 0");
    mbed_mock_set_heap_stats(16384U, 65536U);

    const auto page = syscape::memory::page_size_bytes();
    expect(!page && page.error() == syscape::errc::not_supported,
           "page size must report not_supported on Mbed OS");

    const auto swap = syscape::memory::swap_status();
    expect(!swap && swap.error() == syscape::errc::not_supported,
           "swap status must report not_supported on Mbed OS");

    const auto commit = syscape::memory::commit_status();
    expect(!commit && commit.error() == syscape::errc::not_supported,
           "commit status must report not_supported on Mbed OS");

    const auto huge_size = syscape::memory::huge_page_size_bytes();
    expect(!huge_size && huge_size.error() == syscape::errc::not_supported,
           "huge page size must report not_supported on Mbed OS");

    const auto huge_pool = syscape::memory::huge_page_pool_status();
    expect(!huge_pool && huge_pool.error() == syscape::errc::not_supported,
           "huge page pool must report not_supported on Mbed OS");

    const auto press = syscape::memory::memory_pressure();
    expect(!press && press.error() == syscape::errc::not_supported,
           "memory pressure must report not_supported on Mbed OS");
}

} // namespace

int main() {
    test_memory_queries();
    return failures == 0 ? 0 : 1;
}
