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
    expect(page && *page > 0, "page size must be positive");
    const auto physical = syscape::memory::physical_memory_bytes();
    expect((physical && *physical > 0) ||
               physical.error() == syscape::errc::not_supported,
           "physical memory bytes must be positive or report not_supported");
    const auto available = syscape::memory::available_memory_bytes();
    expect(available.has_value() ||
               available.error() == syscape::errc::not_supported,
           "available memory bytes must succeed or report not_supported");
    const auto load = syscape::memory::memory_load_percent();
    expect((load && *load <= 100) ||
               load.error() == syscape::errc::not_supported,
           "memory load percent must be <= 100 or report not_supported");
}

} // namespace

int main() {
    test_memory_queries();
    return failures == 0 ? 0 : 1;
}
