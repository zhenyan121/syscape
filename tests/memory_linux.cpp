#include <cstdint>
#include <cstdio>
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

void test_bare_count_parser() {
    namespace backend = syscape::detail::memory_backend;
    using syscape::errc;

    const auto plain = backend::parse_bare_count("16");
    expect(plain && *plain == 16U,
           "A documented unitless page count must parse");

    const auto padded = backend::parse_bare_count("   16   ");
    expect(padded && *padded == 16U,
           "Surrounding whitespace around a page count must be tolerated");

    const auto zero = backend::parse_bare_count("0");
    expect(zero && *zero == 0U,
           "A zero pool count is valid data, not an error sentinel");

    const auto suffixed = backend::parse_bare_count("16 kB");
    expect(!suffixed && suffixed.error() == errc::malformed_data,
           "A unit suffix on a count field must be malformed");

    const auto empty = backend::parse_bare_count("   ");
    expect(!empty && empty.error() == errc::malformed_data,
           "An empty count field must be malformed");

    const auto garbage = backend::parse_bare_count("many");
    expect(!garbage && garbage.error() == errc::malformed_data,
           "A nonnumeric count must be malformed");

    const auto partial = backend::parse_bare_count("16 pages");
    expect(!partial && partial.error() == errc::malformed_data,
           "Trailing text after a count must be malformed");
}

void test_huge_page_size_validation() {
    namespace common = syscape::detail::memory_common;
    using syscape::errc;

    const auto valid = common::validate_huge_page_size(
        syscape::result<std::uint64_t>(2ULL * 1024ULL * 1024ULL));
    expect(valid && *valid == 2ULL * 1024ULL * 1024ULL,
           "A positive power-of-two huge-page size must pass validation");

    const auto zero = common::validate_huge_page_size(
        syscape::result<std::uint64_t>(0U));
    expect(!zero && zero.error() == errc::malformed_data,
           "A zero huge-page size must be malformed data");

    const auto non_power_of_two = common::validate_huge_page_size(
        syscape::result<std::uint64_t>(3U * 1024U * 1024U));
    expect(!non_power_of_two &&
               non_power_of_two.error() == errc::malformed_data,
           "A non-power-of-two huge-page size must be malformed data");

    const auto propagated = common::validate_huge_page_size(
        syscape::result<std::uint64_t>(syscape::fail(errc::not_supported)));
    expect(!propagated && propagated.error() == errc::not_supported,
           "Huge-page validation must preserve backend errors");
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

    const std::string extended =
        "Committed_AS:   31599732 kB\n"
        "CommitLimit:    48175404 kB\n"
        "HugePages_Total:       4\n"
        "HugePages_Free:        2\n"
        "Hugepagesize:       2048 kB\n";
    const auto parsed_extended =
        syscape::detail::memory_backend::parse_meminfo(extended);
    expect(parsed_extended && parsed_extended->has_committed &&
               parsed_extended->committed_bytes == 31599732ULL * 1024ULL,
           "Committed_AS must parse into bytes");
    expect(parsed_extended && parsed_extended->has_commit_limit &&
               parsed_extended->commit_limit_bytes == 48175404ULL * 1024ULL,
           "CommitLimit must parse into bytes");
    expect(parsed_extended && parsed_extended->has_huge_total &&
               parsed_extended->huge_total_count == 4U,
           "HugePages_Total must parse as a unitless count");
    expect(parsed_extended && parsed_extended->has_huge_free &&
               parsed_extended->huge_free_count == 2U,
           "HugePages_Free must parse as a unitless count");
    expect(parsed_extended && parsed_extended->has_huge_page_size &&
               parsed_extended->huge_page_size_bytes == 2048ULL * 1024ULL,
           "Hugepagesize must parse into bytes");

    const auto count_with_suffix = syscape::detail::memory_backend::parse_meminfo(
        "HugePages_Total:       4 kB\n");
    expect(!count_with_suffix &&
               count_with_suffix.error() ==
                   syscape::errc::malformed_data,
           "A unit suffix on a count field must be malformed");

    const auto size_without_suffix =
        syscape::detail::memory_backend::parse_meminfo("Hugepagesize: 2048\n");
    expect(!size_without_suffix &&
               size_without_suffix.error() ==
                   syscape::errc::malformed_data,
           "A missing kB suffix on a size field must be malformed");

    const auto missing_fields =
        syscape::detail::memory_backend::parse_meminfo("HugePages_Rsvd:       0\n");
    expect(missing_fields && !missing_fields->has_total &&
               !missing_fields->has_available && !missing_fields->has_swap_total,
           "Unrecognized meminfo fields must not fabricate values");

    const auto skips_unknown = syscape::detail::memory_backend::parse_meminfo(
        "MemTotal: 1024 kB\n"
        "HugePages_Rsvd:    not-a-number\n"
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

void test_micro_percent_parser() {
    namespace backend = syscape::detail::memory_backend;
    using syscape::errc;

    const auto zero = backend::parse_micro_percent("0.00");
    expect(zero && *zero == 0U, "A zero average is valid data");

    const auto plain = backend::parse_micro_percent("12.34");
    expect(plain && *plain == 12340000U,
           "Two fractional digits must scale losslessly into micro-percent");

    const auto maximum = backend::parse_micro_percent("100.00");
    expect(maximum && *maximum == 100000000U,
           "The documented 100-percent bound must be representable");

    const auto single_digit = backend::parse_micro_percent("9.99");
    expect(single_digit && *single_digit == 9990000U,
           "Single-digit percentages must scale identically");

    const auto one_decimal = backend::parse_micro_percent("1.0");
    expect(!one_decimal && one_decimal.error() == errc::malformed_data,
           "One fractional digit is outside the documented rendering");

    const auto three_decimals = backend::parse_micro_percent("1.000");
    expect(!three_decimals && three_decimals.error() == errc::malformed_data,
           "Three fractional digits are outside the documented rendering");

    const auto no_decimal = backend::parse_micro_percent("1");
    expect(!no_decimal && no_decimal.error() == errc::malformed_data,
           "A percentage without a fraction must be malformed");

    const auto leading_dot = backend::parse_micro_percent(".00");
    expect(!leading_dot && leading_dot.error() == errc::malformed_data,
           "An empty integer part must be malformed");

    const auto four_digits = backend::parse_micro_percent("1000.00");
    expect(!four_digits && four_digits.error() == errc::malformed_data,
           "Integer parts beyond the bound must be rejected early");

    const auto beyond_bound = backend::parse_micro_percent("100.01");
    expect(!beyond_bound && beyond_bound.error() == errc::malformed_data,
           "Percentages beyond the documented bound must be malformed");

    const auto negative = backend::parse_micro_percent("-1.00");
    expect(!negative && negative.error() == errc::malformed_data,
           "Negative stall fractions cannot occur and must be malformed");

    const auto garbage = backend::parse_micro_percent("1x.00");
    expect(!garbage && garbage.error() == errc::malformed_data,
           "Nonnumeric integer parts must be malformed");
}

void test_pressure_parser() {
    namespace backend = syscape::detail::memory_backend;
    using syscape::errc;

    const auto both = backend::parse_memory_pressure(
        "some avg10=0.11 avg60=0.22 avg300=0.33 total=111\n"
        "full avg10=0.44 avg60=0.55 avg300=0.66 total=222\n");
    expect(both && both->some.average10_micro_percent == 110000U &&
               both->some.average60_micro_percent == 220000U &&
               both->some.average300_micro_percent == 330000U &&
               both->some.total_microseconds == 111U,
           "The some record must map every documented assignment");
    expect(both && both->has_full &&
               both->full.average10_micro_percent == 440000U &&
               both->full.total_microseconds == 222U,
           "The full record must map every documented assignment");

    const auto missing_full = backend::parse_memory_pressure(
        "some avg10=1.00 avg60=2.00 avg300=3.00 total=42\n");
    expect(!missing_full &&
               missing_full.error() == errc::malformed_data,
           "A memory-pressure snapshot without its documented full record "
           "must be malformed");

    const auto missing_some = backend::parse_memory_pressure(
        "full avg10=0.44 avg60=0.55 avg300=0.66 total=222\n");
    expect(!missing_some &&
               missing_some.error() == errc::malformed_data,
           "A snapshot without a some record cannot satisfy the contract");

    const auto truncated_record = backend::parse_memory_pressure(
        "some avg10=0.11 avg60=0.22 total=111\n");
    expect(!truncated_record &&
               truncated_record.error() == errc::malformed_data,
           "A some record missing an assignment must be malformed");

    const auto malformed_average = backend::parse_memory_pressure(
        "some avg10=0.1 avg60=0.22 avg300=0.33 total=111\n");
    expect(!malformed_average &&
               malformed_average.error() == errc::malformed_data,
           "An undocumented fractional shape must be malformed");

    const auto malformed_total = backend::parse_memory_pressure(
        "some avg10=0.11 avg60=0.22 avg300=0.33 total=later\n");
    expect(!malformed_total && malformed_total.error() == errc::malformed_data,
           "A nonnumeric total must be malformed");

    const auto unknown_kind_skipped = backend::parse_memory_pressure(
        "partial avg10=7.77 avg60=7.77 avg300=7.77 total=777\n"
        "some avg10=0.11 avg60=0.22 avg300=0.33 total=111\n"
        "full avg10=0.44 avg60=0.55 avg300=0.66 total=222\n");
    expect(unknown_kind_skipped &&
               unknown_kind_skipped->some.total_microseconds == 111U,
           "Future kernel record kinds must never break existing queries");

    const auto duplicates_keep_final = backend::parse_memory_pressure(
        "some avg10=0.11 avg60=0.22 avg300=0.33 total=111\n"
        "some avg10=9.90 avg60=0.22 avg300=0.33 total=999\n"
        "full avg10=0.44 avg60=0.55 avg300=0.66 total=222\n");
    expect(duplicates_keep_final &&
               duplicates_keep_final->some.average10_micro_percent == 9900000U &&
               duplicates_keep_final->some.total_microseconds == 999U,
           "Duplicate records keep the final value like sequential reads");

    const auto blank_lines = backend::parse_memory_pressure("\n"
        "some avg10=0.11 avg60=0.22 avg300=0.33 total=111\n\n"
        "full avg10=0.44 avg60=0.55 avg300=0.66 total=222\n\n");
    expect(blank_lines && blank_lines->some.total_microseconds == 111U,
           "Blank lines must be skipped");
}

/// Independently extracts one kibibyte field from a raw meminfo rendering.
std::uint64_t independent_kilobytes(const std::string& content,
                                    const std::string& key) {
    std::size_t offset = 0U;
    while (offset < content.size()) {
        const std::size_t end = content.find('\n', offset);
        const std::string line =
            content.substr(offset, end == std::string::npos
                                       ? std::string::npos
                                       : end - offset);
        offset = end == std::string::npos ? content.size() : end + 1U;
        if (line.rfind(key + ":", 0U) == 0U) {
            return std::stoull(line.substr(key.size() + 1U)) * 1024ULL;
        }
    }
    return 0U;
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

    const auto commit = syscape::memory::commit_status();
    if (commit) {
        // The kernel documents no ordering between Committed_AS and
        // CommitLimit under heuristic overcommit, so only presence and a
        // plausible positive charge are checked here.
        expect(commit->committed_bytes > 0U || commit->commit_limit_bytes > 0U,
               "A Linux commit snapshot must carry recorded accounting");
        std::FILE* const file = std::fopen("/proc/meminfo", "r");
        if (file != nullptr) {
            std::string content;
            char buffer[4096];
            std::size_t count = 0U;
            while ((count = std::fread(buffer, 1U, sizeof(buffer), file)) > 0U) {
                content.append(buffer, count);
            }
            std::fclose(file);
            expect(independent_kilobytes(content, "CommitLimit") ==
                       commit->commit_limit_bytes,
                   "The commit limit must match an independent meminfo read");
            // Committed_AS moves with every allocation, so two reads can
            // never be byte-identical; require agreement within a generous
            // band instead of an exact volatile match.
            const std::uint64_t observed_committed =
                independent_kilobytes(content, "Committed_AS");
            const std::uint64_t recorded_committed = commit->committed_bytes;
            const std::uint64_t drift =
                observed_committed > recorded_committed
                    ? observed_committed - recorded_committed
                    : recorded_committed - observed_committed;
            constexpr std::uint64_t megabyte = 1024ULL * 1024ULL;
            expect(drift <= observed_committed / 20U + 64U * megabyte,
                   "The committed charge must track an independent meminfo "
                   "read within its natural volatility");
        }
    } else {
        expect(commit.error() == syscape::errc::not_found ||
                   commit.error().category() == std::generic_category(),
               "Commit failures must be honest source omissions or native "
               "errors");
    }

    const auto huge_size = syscape::memory::huge_page_size_bytes();
    if (huge_size) {
        expect(*huge_size > 0U && (*huge_size & (*huge_size - 1U)) == 0U,
               "A reported huge-page size must be a positive power of two");
        if (page_size) { expect(*huge_size >= *page_size,
                                "Huge pages are never smaller than base "
                                "pages"); }
    } else {
        expect(huge_size.error() == syscape::errc::not_supported ||
                   huge_size.error() == syscape::errc::not_found,
               "Huge-page-size absence must be an explicit condition");
    }

    const auto pool = syscape::memory::huge_page_pool_status();
    if (pool) {
        expect(pool->free_count <= pool->total_count,
               "Free pool pages must never exceed the configured pool");
    } else {
        expect(pool.error() == syscape::errc::not_supported ||
                   pool.error() == syscape::errc::not_found,
               "Pool-count absence must be an explicit condition");
    }

    const auto load = syscape::memory::memory_load_percent();
    if (load) {
        expect(*load <= 100U,
               "The utilization estimate must stay within its percentage "
               "range");
        if (physical && available) {
            // Recompute the estimate from independent readings; both sides
            // sample continuously changing state, so allow small drift.
            const std::uint64_t used = *physical - *available;
            const std::uint64_t recomputed =
                (*physical == 0U)
                    ? 0U
                    : (used * 100U + *physical / 2U) / *physical;
            const std::uint64_t difference =
                static_cast<std::uint64_t>(*load) > recomputed
                    ? static_cast<std::uint64_t>(*load) - recomputed
                    : recomputed - static_cast<std::uint64_t>(*load);
            expect(difference <= 2U,
                   "The load estimate must track the availability definition");
        }
    } else {
        expect(load.error() == syscape::errc::not_supported ||
                   load.error() == syscape::errc::not_found,
               "Load-estimate absence must be an explicit condition");
    }

    const auto pressure_first = syscape::memory::memory_pressure();
    if (pressure_first) {
        expect(pressure_first->some.average10_micro_percent <= 100000000U &&
                   pressure_first->some.average60_micro_percent <= 100000000U &&
                   pressure_first->some.average300_micro_percent <=
                       100000000U,
               "Stall fractions cannot exceed the 100-percent bound");
        expect(pressure_first->has_full,
               "Linux memory pressure must include its documented full "
               "record");
        const auto pressure_second = syscape::memory::memory_pressure();
        expect(pressure_second &&
                   pressure_second->some.total_microseconds >=
                       pressure_first->some.total_microseconds,
               "Cumulative stall time must never move backwards");
        std::FILE* const file = std::fopen("/proc/pressure/memory", "r");
        if (file != nullptr) {
            std::string content;
            char buffer[4096];
            std::size_t count = 0U;
            while ((count = std::fread(buffer, 1U, sizeof(buffer), file)) > 0U) {
                content.append(buffer, count);
            }
            std::fclose(file);
            const std::size_t some_position = content.find("some ");
            if (some_position != std::string::npos) {
                const std::size_t total_position =
                    content.find("total=", some_position);
                expect(total_position != std::string::npos,
                       "An independent pressure read must contain a total");
                if (total_position != std::string::npos) {
                    const std::uint64_t observed =
                        std::stoull(content.substr(total_position + 6U));
                    expect(observed >= pressure_first->some.total_microseconds,
                           "Recorded stall time must precede later samples");
                }
            }
        }
    } else {
        // Kernels without pressure information expose no file at all.
        expect(pressure_first.error() == syscape::errc::not_supported ||
                   pressure_first.error().category() ==
                       std::generic_category(),
               "Pressure absence must be not_supported or a native error");
    }
}

} // namespace

int main() {
    test_kilobyte_parser();
    test_bare_count_parser();
    test_huge_page_size_validation();
    test_meminfo_parser();
    test_micro_percent_parser();
    test_pressure_parser();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
