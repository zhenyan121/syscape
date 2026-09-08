#include <cstdint>
#include <iostream>
#include <limits>

#include <syscape/memory.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_scaled_memory() {
    using syscape::detail::memory_backend::nuttx_scaled_memory;
    const auto value = nuttx_scaled_memory(1024UL, 4U);
    expect(value && *value == 4096U, "memory units must be converted to bytes");

    const auto zero_unit = nuttx_scaled_memory(1UL, 0U);
    expect(!zero_unit && zero_unit.error() == syscape::errc::malformed_data,
           "a zero memory unit must be rejected");

    if (sizeof(unsigned long) >= sizeof(std::uint64_t)) {
        const auto overflow = nuttx_scaled_memory(
            (std::numeric_limits<unsigned long>::max)(), 2U);
        expect(!overflow && overflow.error() == syscape::errc::value_too_large,
               "memory unit multiplication must reject overflow");
    }
}

void test_memory_queries() {
    const auto page = syscape::memory::page_size_bytes();
    expect(page && *page > 0U && (*page & (*page - 1U)) == 0U,
           "page size must be a positive power of two");

    const auto total = syscape::memory::physical_memory_bytes();
    const auto available = syscape::memory::available_memory_bytes();
    expect(total && available && *total > 0U && *available <= *total,
           "NuttX sysinfo memory values must form a valid snapshot");

    const auto swap = syscape::memory::swap_status();
    expect(!swap && swap.error() == syscape::errc::not_supported,
           "zero-filled NuttX swap fields must not be exposed as observations");

    const auto load = syscape::memory::memory_load_percent();
    expect(load && *load <= 100U, "memory load must be a valid percentage");

    expect(syscape::memory::commit_status().error() ==
               syscape::errc::not_supported,
           "commit accounting must report not_supported on NuttX");
}

} // namespace

int main() {
    test_scaled_memory();
    test_memory_queries();
    return failures == 0 ? 0 : 1;
}
