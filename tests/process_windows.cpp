#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/process.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_filetime_conversion() {
    using syscape::detail::process_backend::filetime_to_time_point;
    using syscape::detail::process_backend::
        hundred_nanosecond_units_to_duration;
    using syscape::detail::process_backend::filetime_units;

    const auto amount = hundred_nanosecond_units_to_duration(5U);
    expect(amount && amount->count() == 500LL,
           "Hundred-nanosecond units must convert to nanoseconds");

    const auto oversized_amount = hundred_nanosecond_units_to_duration(
        static_cast<std::uint64_t>(
            (std::chrono::nanoseconds::max)().count()) /
            100U +
        1U);
    expect(!oversized_amount && oversized_amount.error() ==
                                   syscape::errc::value_too_large,
           "Durations beyond the nanoseconds maximum must be reported as "
           "too large");

    constexpr std::uint64_t january_2021_units = 132539328000000000ULL;
    ::FILETIME value {};
    value.dwHighDateTime =
        static_cast<::DWORD>(january_2021_units >> 32U);
    value.dwLowDateTime =
        static_cast<::DWORD>(january_2021_units & 0xffffffffU);
    expect(filetime_units(value) == january_2021_units,
           "FILETIME halves must reassemble into the original count");

    const auto converted = filetime_to_time_point(value);
    const std::chrono::system_clock::time_point expected(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::seconds(1609459200)));
    expect(converted && *converted == expected,
           "A known FILETIME must convert to the matching Unix instant");

    ::FILETIME before_epoch {};
    before_epoch.dwHighDateTime =
        static_cast<::DWORD>(116444735999999999ULL >> 32U);
    before_epoch.dwLowDateTime =
        static_cast<::DWORD>(116444735999999999ULL & 0xffffffffU);
    const auto rejected = filetime_to_time_point(before_epoch);
    expect(!rejected && rejected.error() ==
                            syscape::errc::malformed_data,
           "FILETIME values before the Unix epoch are malformed data");

    const ::FILETIME zero {};
    const auto zero_rejected = filetime_to_time_point(zero);
    expect(!zero_rejected && zero_rejected.error() ==
                                 syscape::errc::malformed_data,
           "A zero FILETIME cannot describe a process creation instant");
}

void test_address_space_walk() {
    using syscape::detail::process_backend::region_description;
    using syscape::detail::process_backend::sum_address_space;

    const auto mixed = sum_address_space(
        [](std::uintptr_t address) ->
            syscape::result<region_description> {
            syscape::result<region_description> described;
            region_description& region = described.value();
            if (address == 0U) {
                region.reserved_or_committed = false;
                region.size_bytes = 100U;
            } else if (address == 100U) {
                region.reserved_or_committed = true;
                region.size_bytes = 200U;
            } else if (address == 300U) {
                region.reserved_or_committed = false;
                region.size_bytes = 100U;
            } else if (address == 400U) {
                region.reserved_or_committed = true;
                region.size_bytes = 300U;
            } else {
                region.reserved_or_committed = true;
                region.size_bytes = 1000U;
            }
            return described;
        },
        0U, 1000U);
    expect(mixed && *mixed == 1500U,
           "The walk must sum reserved and committed regions and stop at "
           "the maximum address");

    const auto zero_sized = sum_address_space(
        [](std::uintptr_t) -> syscape::result<region_description> {
            syscape::result<region_description> described;
            described.value().size_bytes = 0U;
            return described;
        },
        0U, 1000U);
    expect(!zero_sized && zero_sized.error() ==
                              syscape::errc::malformed_data,
           "A zero-sized region cannot make progress and is malformed");

    std::uint32_t oversized_query_count = 0U;
    const auto oversized_region = sum_address_space(
        [&oversized_query_count](std::uintptr_t) ->
            syscape::result<region_description> {
            ++oversized_query_count;
            syscape::result<region_description> described;
            described.value().size_bytes =
                (std::numeric_limits<std::uint64_t>::max)();
            return described;
        },
        100U, 1000U);
    expect(oversized_region && *oversized_region == 0U &&
               oversized_query_count == 1U,
           "A region larger than the remaining address range must stop the "
           "walk without wrapping the next address");

    const auto failing = sum_address_space(
        [](std::uintptr_t address) ->
            syscape::result<region_description> {
            if (address == 100U) {
                return syscape::result<region_description>(
                    syscape::fail(syscape::errc::io_error));
            }
            syscape::result<region_description> described;
            described.value().size_bytes = 100U;
            return described;
        },
        0U, 1000U);
    expect(!failing && failing.error() == syscape::errc::io_error,
           "A failing region query must surface its native error");

    const auto overflowing = sum_address_space(
        [](std::uintptr_t) -> syscape::result<region_description> {
            syscape::result<region_description> described;
            region_description& region = described.value();
            region.reserved_or_committed = true;
            region.size_bytes = (std::numeric_limits<std::uint64_t>::max)();
            return described;
        },
        0U, (std::numeric_limits<std::uint64_t>::max)());
    expect(!overflowing && overflowing.error() ==
                               syscape::errc::value_too_large,
           "Region totals beyond 64 bits must be reported as too large");
}

} // namespace

int main() {
    test_filetime_conversion();
    test_address_space_walk();

    const auto id = syscape::process::process_id();
    expect(id && *id > 0U, "Windows must report a positive process ID");

    const auto parent = syscape::process::parent_process_id();
    expect(parent.has_value(),
           "Windows must locate the current process in the Toolhelp snapshot");

    const auto executable = syscape::process::executable_path();
    expect(executable && !executable->empty() &&
               syscape::detail::is_valid_utf8(*executable),
           "Windows must convert GetModuleFileNameW output to UTF-8");
    if (executable) {
        expect(std::filesystem::path(*executable).is_absolute(),
               "Windows executable paths must be absolute");
    }

    const auto arguments = syscape::process::command_line();
    expect(arguments && !arguments->empty(),
           "Windows must split CommandLineToArgvW output into UTF-8 values");

    const auto working_directory = syscape::process::working_directory();
    expect(working_directory && !working_directory->empty() &&
               std::filesystem::path(*working_directory).is_absolute(),
           "Windows must report an absolute working directory");

    const auto cpu = syscape::process::cpu_time();
    expect(cpu && cpu->user >= std::chrono::nanoseconds::zero() &&
               cpu->system >= std::chrono::nanoseconds::zero(),
           "Windows must report nonnegative GetProcessTimes durations");

    const auto started = syscape::process::start_time();
    const std::chrono::system_clock::time_point year_2000(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::seconds(946684800)));
    expect(started && *started >= year_2000 &&
               *started <= std::chrono::system_clock::now(),
           "Windows must report the process creation instant");

    const auto memory = syscape::process::memory_usage();
    expect(memory && memory->resident_bytes > 0U &&
               memory->virtual_bytes > 0U,
           "Windows must report nonzero working-set and address-space "
           "extents");

    const auto threads = syscape::process::thread_count();
    expect(threads && *threads >= 1U,
           "Windows must count at least the calling thread");

    return failures == 0 ? 0 : 1;
}
