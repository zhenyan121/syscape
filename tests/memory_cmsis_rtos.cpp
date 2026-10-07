#include <iostream>

#include "cmsis_os2.h"

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
#if defined(CMSIS_RTOS_TOTAL_HEAP_SIZE) && defined(CMSIS_RTOS_FREE_HEAP_SIZE)
    const auto phys = syscape::memory::physical_memory_bytes();
    expect(phys.has_value() && *phys == CMSIS_RTOS_TOTAL_HEAP_SIZE,
           "physical memory must report CMSIS_RTOS_TOTAL_HEAP_SIZE");

    const auto avail = syscape::memory::available_memory_bytes();
    expect(avail.has_value() && *avail == CMSIS_RTOS_FREE_HEAP_SIZE,
           "available memory must report CMSIS_RTOS_FREE_HEAP_SIZE");

    const auto load = syscape::memory::memory_load_percent();
    expect(load.has_value(),
           "memory load percent must succeed when heap size is configured");
#if (CMSIS_RTOS_FREE_HEAP_SIZE == 0)
    expect(load.has_value() && *load == 100,
           "memory load percent must report 100 when free heap is 0");
#endif
#else
    const auto phys = syscape::memory::physical_memory_bytes();
    expect(!phys && phys.error() == syscape::errc::not_supported,
           "physical memory must report not_supported when heap size is "
           "unconfigured");

    const auto avail = syscape::memory::available_memory_bytes();
    expect(!avail && avail.error() == syscape::errc::not_supported,
           "available memory must report not_supported when heap size is "
           "unconfigured");

    const auto load = syscape::memory::memory_load_percent();
    expect(!load && load.error() == syscape::errc::not_supported,
           "memory load percent must report not_supported when heap size is "
           "unconfigured");
#endif

    const auto page = syscape::memory::page_size_bytes();
    expect(!page && page.error() == syscape::errc::not_supported,
           "page size must report not_supported on CMSIS-RTOS");

    const auto swap = syscape::memory::swap_status();
    expect(!swap && swap.error() == syscape::errc::not_supported,
           "swap status must report not_supported on CMSIS-RTOS");

    const auto commit = syscape::memory::commit_status();
    expect(!commit && commit.error() == syscape::errc::not_supported,
           "commit status must report not_supported on CMSIS-RTOS");

    const auto huge = syscape::memory::huge_page_size_bytes();
    expect(!huge && huge.error() == syscape::errc::not_supported,
           "huge page size must report not_supported on CMSIS-RTOS");
}

} // namespace

int main() {
    test_memory_queries();
    return failures == 0 ? 0 : 1;
}
