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
    const auto page = syscape::memory::page_size_bytes();
    expect(page.has_value() || page.error() == syscape::errc::not_supported,
           "page_size_bytes must succeed or report not_supported on WASI");
    if (page.has_value()) {
        expect(*page > 0U, "page size when reported must be positive");
    }

    const auto phys = syscape::memory::physical_memory_bytes();
    expect(!phys && phys.error() == syscape::errc::not_supported,
           "physical memory must report not_supported on WASI sandbox");

    const auto avail = syscape::memory::available_memory_bytes();
    expect(!avail && avail.error() == syscape::errc::not_supported,
           "available memory must report not_supported on WASI sandbox");

    const auto swap = syscape::memory::swap_status();
    expect(!swap && swap.error() == syscape::errc::not_supported,
           "swap status must report not_supported on WASI");

    const auto load = syscape::memory::memory_load_percent();
    expect(!load && load.error() == syscape::errc::not_supported,
           "memory load percent must report not_supported on WASI");
}

} // namespace

int main() {
    test_memory_queries();
    return failures == 0 ? 0 : 1;
}
