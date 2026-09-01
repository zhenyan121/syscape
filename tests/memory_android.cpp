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
    const auto total = syscape::memory::physical_memory_bytes();
    expect(total && *total > 0, "total physical memory must be positive");

    const auto free_bytes = syscape::memory::available_memory_bytes();
    expect(free_bytes.has_value(), "available memory query must succeed");
    if (free_bytes && total) {
        expect(*free_bytes <= *total,
               "available memory must not exceed total physical memory");
    }

    const auto page_size = syscape::memory::page_size_bytes();
    expect(page_size && *page_size > 0, "page size must be positive");

    const auto swap = syscape::memory::swap_status();
    expect(swap.has_value() || swap.error() == syscape::errc::not_supported,
           "swap status query must succeed or report not_supported");
}

} // namespace

int main() {
    test_memory_queries();
    return failures == 0 ? 0 : 1;
}
