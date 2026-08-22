#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>

#include <syscape/detail/memory/linux.hpp>
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
    const auto plain = syscape::detail::memory_backend::parse_kilobyte_amount(
        "16384252 kB");
    expect(plain && *plain == 16384252ULL * 1024ULL,
           "A documented kilobyte amount must convert to bytes");

    const auto bare = syscape::detail::memory_backend::parse_kilobyte_amount(
        "  4096  ");
    expect(!bare && bare.error() == syscape::errc::malformed_data,
           "A recognized amount without the documented kB suffix must be "
           "malformed");

    const auto zero = syscape::detail::memory_backend::parse_kilobyte_amount(
        "0 kB");
    expect(zero && *zero == 0U, "Zero swap capacity is valid data, not an error");

    const auto empty =
        syscape::detail::memory_backend::parse_kilobyte_amount("   ");
    expect(!empty && empty.error() == syscape::errc::malformed_data,
           "An empty amount must be malformed");

    const auto garbage =
        syscape::detail::memory_backend::parse_kilobyte_amount("abc kB");
    expect(!garbage && garbage.error() == syscape::errc::malformed_data,
           "A nonnumeric amount must be malformed");

    const auto trailing = syscape::detail::memory_backend::parse_kilobyte_amount(
        "12 kB extra");
    expect(!trailing && trailing.error() == syscape::errc::malformed_data,
           "Trailing text after the documented suffix must be malformed");

    const auto wrong_unit =
        syscape::detail::memory_backend::parse_kilobyte_amount("12 KB");
    expect(!wrong_unit && wrong_unit.error() == syscape::errc::malformed_data,
           "Only the kernel-documented kB suffix is accepted");

    const auto overflow = syscape::detail::memory_backend::parse_kilobyte_amount(
        "18014398509481984 kB");
    expect(!overflow && overflow.error() == syscape::errc::value_too_large,
           "Kilobyte amounts beyond uint64 bytes must fail");
}

void test_meminfo_parser() {
    const std::string input =
        "MemTotal:       16384252 kB\n"
        "MemFree:         1024000 kB\n"
        "MemAvailable:    8388608 kB\n"
        "Buffers:          524288 kB\n"
        "SwapTotal:             0 kB\n"
        "SwapFree:              0 kB\n";
    const auto parsed = syscape::detail::memory_backend::parse_meminfo(input);
    expect(parsed && parsed->has_total &&
               parsed->total_bytes == 16384252ULL * 1024ULL,
           "MemTotal must parse into bytes");
    expect(parsed && parsed->has_available &&
               parsed->available_bytes == 8388608ULL * 1024ULL,
           "MemAvailable must parse into bytes");
    expect(parsed && parsed->has_swap_total && parsed->swap_total_bytes == 0U,
           "SwapTotal of zero is valid data");
    expect(parsed && parsed->has_swap_free && parsed->swap_free_bytes == 0U,
           "SwapFree of zero is valid data");

    const auto missing_fields =
        syscape::detail::memory_backend::parse_meminfo("HugePages_Total:       0\n");
    expect(missing_fields && !missing_fields->has_total &&
               !missing_fields->has_available && !missing_fields->has_swap_total,
           "Unrecognized meminfo fields must not fabricate values");

    const auto skips_unknown = syscape::detail::memory_backend::parse_meminfo(
        "MemTotal: 1024 kB\n"
        "HugePages_Total:    not-a-number\n"
        "SomeFutureField: ??\n");
    expect(skips_unknown && skips_unknown->has_total &&
               skips_unknown->total_bytes == 1024ULL * 1024ULL,
           "Unrecognized fields must be skipped before value parsing");

    const auto malformed = syscape::detail::memory_backend::parse_meminfo(
        "MemTotal:\n");
    expect(!malformed && malformed.error() == syscape::errc::malformed_data,
           "A recognized field with an empty amount must be malformed");

    const auto malformed_suffix =
        syscape::detail::memory_backend::parse_meminfo("MemTotal: 12 KB\n");
    expect(!malformed_suffix &&
               malformed_suffix.error() == syscape::errc::malformed_data,
           "A recognized field with a wrong unit must be malformed");

    const auto unknown_wrong_unit =
        syscape::detail::memory_backend::parse_meminfo("MemFree: 12 KB\n");
    expect(unknown_wrong_unit && !unknown_wrong_unit->has_total,
           "An unrecognized field with any format must be skipped");

    const auto no_newline = syscape::detail::memory_backend::parse_meminfo(
        "MemAvailable: 4096 kB");
    expect(no_newline && no_newline->has_available &&
               no_newline->available_bytes == 4096ULL * 1024ULL,
           "The final line without a newline must still parse");
}

void test_runtime_queries() {
    const auto page_size = syscape::memory::page_size_bytes();
    expect(page_size && *page_size > 0U && (*page_size & (*page_size - 1U)) == 0U,
           "Linux must report a positive power-of-two page size");

    const auto physical = syscape::memory::physical_memory_bytes();
    expect(physical && *physical > 0U,
           "Linux must report positive physical memory from /proc/meminfo");

    const auto available = syscape::memory::available_memory_bytes();
    if (available) {
        expect(physical && *available <= *physical,
               "Available memory must not exceed physical memory");
    } else {
        // Kernels without MemAvailable report not_supported; any other
        // failure must be a native platform error, never a fabricated value.
        expect(!physical || available.error() != syscape::errc::not_found,
               "A present /proc/meminfo source must not report not_found");
    }

    const auto swap = syscape::memory::swap_status();
    if (swap) {
        expect(swap->free_bytes <= swap->total_bytes,
               "Unused swap capacity must never exceed total capacity");
    }
}

} // namespace

int main() {
    test_kilobyte_parser();
    test_meminfo_parser();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
