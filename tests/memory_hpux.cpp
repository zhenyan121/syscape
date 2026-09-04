#include <iostream>
#include <cstdlib>

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
    expect(swap.has_value(), "swap query must succeed with pstat");
    if (swap) {
        expect(swap->total_bytes == 3221225472ULL,
               "block and filesystem swap totals must use HP-UX units");
        expect(swap->free_bytes == 1610612736ULL,
               "swap free pages must use the runtime page size");
        expect(swap->free_bytes <= swap->total_bytes,
               "swap free bytes must not exceed total swap");
    }

    const auto commit = syscape::memory::commit_status();
    expect(commit.error() == syscape::errc::not_supported,
           "commit_status query must report not_supported on HP-UX");

    const auto load = syscape::memory::memory_load_percent();
    expect(load.has_value(),
           "memory load query must succeed on HP-UX with pstat");
    if (load) {
        expect(*load <= 100, "memory load percent must be <= 100");
    }

#if defined(SYSCAPE_HPUX_PSTAT_MOCK)
    ::setenv("SYSCAPE_TEST_PSTAT_STATIC_ZERO", "1", 1);
    expect(syscape::memory::physical_memory_bytes().error() ==
               syscape::errc::temporarily_unavailable,
           "an empty static snapshot must not fabricate physical memory");
    expect(syscape::memory::available_memory_bytes().error() ==
               syscape::errc::temporarily_unavailable,
           "an empty static snapshot must not fabricate available memory");
    expect(syscape::memory::swap_status().error() ==
               syscape::errc::temporarily_unavailable,
           "an empty static snapshot must not produce swap data");
    ::unsetenv("SYSCAPE_TEST_PSTAT_STATIC_ZERO");

    ::setenv("SYSCAPE_TEST_PSTAT_DYNAMIC_ZERO", "1", 1);
    expect(syscape::memory::available_memory_bytes().error() ==
               syscape::errc::temporarily_unavailable,
           "an empty dynamic snapshot must not fabricate available memory");
    ::unsetenv("SYSCAPE_TEST_PSTAT_DYNAMIC_ZERO");

    ::setenv("SYSCAPE_TEST_PSTAT_PHYSICAL_INVALID", "1", 1);
    expect(syscape::memory::physical_memory_bytes().error() ==
               syscape::errc::malformed_data,
           "an invalid physical page count must report malformed_data");
    ::unsetenv("SYSCAPE_TEST_PSTAT_PHYSICAL_INVALID");

    ::setenv("SYSCAPE_TEST_PSTAT_PAGE_SIZE_INVALID", "1", 1);
    expect(syscape::memory::physical_memory_bytes().error() ==
               syscape::errc::malformed_data,
           "an invalid pstat page size must not fall back to sysconf");
    expect(syscape::memory::available_memory_bytes().error() ==
               syscape::errc::malformed_data,
           "available memory must reject an invalid pstat page size");
    ::unsetenv("SYSCAPE_TEST_PSTAT_PAGE_SIZE_INVALID");

    ::setenv("SYSCAPE_TEST_PSTAT_DYNAMIC_INVALID", "1", 1);
    expect(syscape::memory::available_memory_bytes().error() ==
               syscape::errc::malformed_data,
           "a negative free page count must report malformed_data");
    ::unsetenv("SYSCAPE_TEST_PSTAT_DYNAMIC_INVALID");

    ::setenv("SYSCAPE_TEST_PSTAT_SWAP_BACKWARD", "1", 1);
    expect(syscape::memory::swap_status().error() ==
               syscape::errc::malformed_data,
           "swap enumeration must reject a non-advancing index");
    ::unsetenv("SYSCAPE_TEST_PSTAT_SWAP_BACKWARD");
#endif
}

} // namespace

int main() {
    test_memory_queries();
    return failures == 0 ? 0 : 1;
}
