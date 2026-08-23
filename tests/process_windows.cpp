#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>
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

void test_priority_class_mapping() {
    using syscape::detail::process_backend::map_priority_class;

    const struct {
        ::DWORD native;
        int expected;
    } classes[] = {
        {IDLE_PRIORITY_CLASS, 4},
        {BELOW_NORMAL_PRIORITY_CLASS, 6},
        {NORMAL_PRIORITY_CLASS, 8},
        {ABOVE_NORMAL_PRIORITY_CLASS, 10},
        {HIGH_PRIORITY_CLASS, 13},
        {REALTIME_PRIORITY_CLASS, 24},
    };

    for (const auto& item : classes) {
        const auto mapped = map_priority_class(item.native);
        expect(mapped && *mapped == item.expected,
               "Each documented priority class must map onto its "
               "documented base priority");
    }

    const auto unknown = map_priority_class(0U);
    expect(!unknown && unknown.error() == syscape::errc::malformed_data,
           "An unrecognized priority class cannot come from the "
           "documented source and is malformed");
}

void test_affinity_expansion() {
    using syscape::detail::process_backend::expand_affinity_mask;
    using syscape::detail::process_backend::validate_processor_group_count;

    const auto no_groups = validate_processor_group_count(0U);
    expect(!no_groups && no_groups.error() == syscape::errc::malformed_data,
           "Windows must expose at least one processor group");

    const auto one_group = validate_processor_group_count(1U);
    expect(one_group.has_value(),
           "One processor group has unambiguous system-wide indices");

    const auto multiple_groups = validate_processor_group_count(2U);
    expect(!multiple_groups &&
               multiple_groups.error() == syscape::errc::not_supported,
           "Group-relative affinity is ambiguous on multiple-group systems");

    const auto scattered = expand_affinity_mask(0b0101ULL, 0b0111ULL);
    expect(scattered && scattered->size() == 2U && (*scattered)[0] == 0U &&
               (*scattered)[1] == 2U,
           "Process-mask bits must expand into ascending indices");

    const auto highest_bit =
        expand_affinity_mask(1ULL << 63U, ~(0ULL));
    expect(highest_bit && highest_bit->size() == 1U &&
               (*highest_bit)[0] == 63U,
           "The topmost group bit must keep its documented index");

    const auto beyond_system = expand_affinity_mask(0b1000ULL, 0b0111ULL);
    expect(!beyond_system &&
               beyond_system.error() == syscape::errc::malformed_data,
           "A process mask beyond the system mask is malformed data");

    const auto empty_process = expand_affinity_mask(0ULL, ~0ULL);
    expect(!empty_process &&
               empty_process.error() == syscape::errc::malformed_data,
           "An empty process mask cannot describe a runnable process");
}

} // namespace

int main() {
    test_filetime_conversion();
    test_address_space_walk();
    test_priority_class_mapping();
    test_affinity_expansion();

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

    const auto scheduling = syscape::process::priority();
    expect(scheduling.has_value(),
           "Windows must report a priority from GetPriorityClass");
    if (scheduling) {
        const bool documented = *scheduling == 4 || *scheduling == 6 ||
                                *scheduling == 8 || *scheduling == 10 ||
                                *scheduling == 13 || *scheduling == 24;
        expect(documented,
               "The reported priority must be a documented base priority");
    }

    ::ULONG_PTR reference_process_mask = 0U;
    ::ULONG_PTR reference_system_mask = 0U;
    const ::WORD group_count = ::GetActiveProcessorGroupCount();
    const auto indices = syscape::process::cpu_affinity();
    if (group_count == 1U) {
        expect(indices && !indices->empty(),
               "Single-group Windows must expand a nonempty affinity mask");
    } else if (group_count > 1U) {
        expect(!indices && indices.error() == syscape::errc::not_supported,
               "Multiple-group Windows must reject ambiguous affinity indices");
    } else {
        expect(!indices && indices.error() == syscape::errc::malformed_data,
               "A zero processor-group count must be malformed data");
    }
    if (group_count == 1U && indices &&
        ::GetProcessAffinityMask(::GetCurrentProcess(),
                                 &reference_process_mask,
                                 &reference_system_mask) != 0) {
        std::vector<std::uint32_t> reference_indices;
        for (std::uint32_t bit = 0U; bit < 64U; ++bit) {
            const auto mask_bit =
                static_cast<std::uint64_t>(1ULL) << bit;
            if ((static_cast<std::uint64_t>(reference_process_mask) &
                 mask_bit) != 0U) {
                reference_indices.push_back(bit);
            }
        }
        expect(*indices == reference_indices,
               "cpu_affinity() must match an independent "
               "GetProcessAffinityMask call");
    }

    const auto unsupported_limits = syscape::process::resource_limit(
        syscape::process::resource_kind::open_files);
    expect(!unsupported_limits &&
               unsupported_limits.error() ==
                   std::errc::operation_not_supported,
           "Windows records no public per-process resource limits and "
           "must report them as unsupported");

    return failures == 0 ? 0 : 1;
}
