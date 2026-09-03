#include <cstdint>
#include <iostream>

#include <syscape/detail/memory/openharmony.hpp>
#include <syscape/memory.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_kilobyte_parser() {
    const auto plain =
        syscape::detail::memory_backend::parse_kilobyte_amount("16384252 kB");
    expect(plain && *plain == 16384252ULL * 1024ULL,
           "A documented kilobyte amount must convert to bytes");

    const auto zero =
        syscape::detail::memory_backend::parse_kilobyte_amount("0 kB");
    expect(zero && *zero == 0U,
           "Zero kilobyte amount is valid data, not an error");

    const auto empty =
        syscape::detail::memory_backend::parse_kilobyte_amount("   ");
    expect(!empty && empty.error() == syscape::errc::malformed_data,
           "An empty amount must be malformed");

    const auto garbage =
        syscape::detail::memory_backend::parse_kilobyte_amount("abc kB");
    expect(!garbage && garbage.error() == syscape::errc::malformed_data,
           "A nonnumeric amount must be malformed");

    const auto wrong_unit =
        syscape::detail::memory_backend::parse_kilobyte_amount("12 KB");
    expect(!wrong_unit && wrong_unit.error() == syscape::errc::malformed_data,
           "Only the kernel-documented kB suffix is accepted");

    const auto trailing =
        syscape::detail::memory_backend::parse_kilobyte_amount("12 kB extra");
    expect(!trailing && trailing.error() == syscape::errc::malformed_data,
           "Trailing text after the documented suffix must be malformed");

    const auto mult_overflow =
        syscape::detail::memory_backend::parse_kilobyte_amount(
            "18014398509481984 kB");
    expect(
        !mult_overflow &&
            mult_overflow.error() == syscape::errc::value_too_large,
        "Kilobyte amounts beyond uint64 bytes must fail with value_too_large");

    const auto range_overflow =
        syscape::detail::memory_backend::parse_kilobyte_amount(
            "9999999999999999999999999999999999999999 kB");
    expect(!range_overflow &&
               range_overflow.error() == syscape::errc::value_too_large,
           "Amounts exceeding integer range must fail with value_too_large");
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
    test_kilobyte_parser();
    test_memory_queries();
    return failures == 0 ? 0 : 1;
}
