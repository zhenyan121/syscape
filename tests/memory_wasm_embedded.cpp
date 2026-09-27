#include <iostream>
#include <cstdint>

#include <syscape/execution_environment.hpp>
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
    expect(syscape::target_operating_system() ==
               syscape::operating_system::embedded_wasm,
           "target_operating_system must report embedded_wasm");

    const auto page = syscape::memory::page_size_bytes();
    expect(page.has_value(), "page_size_bytes must succeed");
    if (page.has_value()) {
        expect(*page == 65536ULL || (*page & (*page - 1ULL)) == 0ULL,
               "page_size_bytes must be 64 KiB or valid power of two");
    }

    const auto phys = syscape::memory::physical_memory_bytes();
    expect(!phys && phys.error() == syscape::errc::not_supported,
           "physical_memory_bytes must report not_supported on Embedded "
           "WebAssembly");

    const auto avail = syscape::memory::available_memory_bytes();
    expect(!avail && avail.error() == syscape::errc::not_supported,
           "available_memory_bytes must report not_supported on Embedded "
           "WebAssembly");

    const auto swap = syscape::memory::swap_status();
    expect(!swap && swap.error() == syscape::errc::not_supported,
           "swap_status must report not_supported on Embedded WebAssembly");

    const auto load = syscape::memory::memory_load_percent();
    expect(!load && load.error() == syscape::errc::not_supported,
           "memory_load_percent must report not_supported on Embedded "
           "WebAssembly");

    const auto pressure = syscape::memory::memory_pressure();
    expect(!pressure && pressure.error() == syscape::errc::not_supported,
           "memory_pressure must report not_supported on Embedded WebAssembly");
}

} // namespace

int main() {
    test_memory_queries();
    return failures == 0 ? 0 : 1;
}
