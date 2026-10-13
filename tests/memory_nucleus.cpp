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
#if defined(SYSCAPE_NUCLEUS_TOTAL_HEAP_SIZE) &&                                \
    defined(SYSCAPE_NUCLEUS_FREE_HEAP_SIZE)
    constexpr std::uint64_t expected_total = SYSCAPE_NUCLEUS_TOTAL_HEAP_SIZE;
    constexpr std::uint64_t expected_free = SYSCAPE_NUCLEUS_FREE_HEAP_SIZE;
#define SYSCAPE_TEST_NUCLEUS_HEAP_CONFIGURED 1
#elif defined(NUCLEUS_TOTAL_HEAP_SIZE) && defined(NUCLEUS_FREE_HEAP_SIZE)
    constexpr std::uint64_t expected_total = NUCLEUS_TOTAL_HEAP_SIZE;
    constexpr std::uint64_t expected_free = NUCLEUS_FREE_HEAP_SIZE;
#define SYSCAPE_TEST_NUCLEUS_HEAP_CONFIGURED 1
#endif

#if defined(SYSCAPE_TEST_NUCLEUS_HEAP_CONFIGURED)
    const auto phys = syscape::memory::physical_memory_bytes();
    expect(phys.has_value() && *phys == expected_total,
           "physical memory must report total heap size");

    const auto avail = syscape::memory::available_memory_bytes();
    expect(avail.has_value() && *avail == expected_free,
           "available memory must report free heap size");

    const auto load = syscape::memory::memory_load_percent();
    expect(load.has_value(),
           "memory load percent must succeed when heap size is configured");
    if (expected_free == 0) {
        expect(load.has_value() && *load == 100,
               "memory load percent must report 100 when free heap is 0");
    } else if (expected_free * 2 == expected_total) {
        expect(load.has_value() && *load == 50,
               "memory load percent must report 50 when half heap is free");
    }
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
           "page size must report not_supported on Nucleus RTOS");

    const auto swap = syscape::memory::swap_status();
    expect(!swap && swap.error() == syscape::errc::not_supported,
           "swap status must report not_supported on Nucleus RTOS");
}

} // namespace

int main() {
    test_memory_queries();
    return failures == 0 ? 0 : 1;
}
