#include <chrono>
#include <cstdint>
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

void test_conversion_helpers() {
    using syscape::detail::process_backend::nanoseconds_amount;
    using syscape::detail::process_backend::task_thread_count;
    using syscape::detail::process_backend::timeval_components_to_time_point;

    const auto amount = nanoseconds_amount(5U);
    expect(amount && amount->count() == 5LL,
           "Platform nanosecond counts must convert to durations");

    const auto oversized = nanoseconds_amount(
        static_cast<std::uint64_t>(
            (std::chrono::nanoseconds::max)().count()) +
        1ULL);
    expect(!oversized && oversized.error() ==
                             syscape::errc::value_too_large,
           "Nanosecond counts beyond the duration maximum must be "
           "reported as too large");

    const auto converted = timeval_components_to_time_point(1609459200,
                                                            500000);
    const std::chrono::system_clock::time_point expected(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::seconds(1609459200) +
            std::chrono::microseconds(500000)));
    expect(converted && *converted == expected,
           "A known start-time pair must convert to the matching instant");

    const auto negative_seconds =
        timeval_components_to_time_point(-1, 0);
    expect(!negative_seconds && negative_seconds.error() ==
                                    syscape::errc::malformed_data,
           "Negative start-time seconds are malformed platform data");

    const auto out_of_range_micros =
        timeval_components_to_time_point(1609459200, 1000000);
    expect(!out_of_range_micros && out_of_range_micros.error() ==
                                       syscape::errc::malformed_data,
           "Microsecond fields outside one second are malformed data");

    using clock = std::chrono::system_clock;
    const auto maximum_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(
            clock::duration::max()).count();
    const auto overflowing_fraction =
        timeval_components_to_time_point(maximum_seconds, 999999);
    expect(!overflowing_fraction && overflowing_fraction.error() ==
                                        syscape::errc::value_too_large,
           "A fractional component beyond the clock maximum must be "
           "reported as too large");

    const auto positive_threads = task_thread_count(5);
    expect(positive_threads && *positive_threads == 5U,
           "A positive native thread count must be preserved");
    const auto zero_threads = task_thread_count(0);
    const auto negative_threads = task_thread_count(-1);
    expect(!zero_threads && !negative_threads &&
               zero_threads.error() == syscape::errc::malformed_data &&
               negative_threads.error() == syscape::errc::malformed_data,
           "Nonpositive native thread counts must be malformed data");
}

} // namespace

int main() {
    test_conversion_helpers();

    const auto id = syscape::process::process_id();
    expect(id && *id > 0U, "macOS must report a positive process ID");

    const auto parent = syscape::process::parent_process_id();
    expect(parent.has_value(), "macOS must query the parent process ID");

    const auto executable = syscape::process::executable_path();
    expect(executable && !executable->empty() &&
               executable->front() == '/' &&
               syscape::detail::is_valid_utf8(*executable),
           "macOS must report an absolute UTF-8 executable path");

    const auto arguments = syscape::process::command_line();
    expect(arguments && !arguments->empty(),
           "macOS must expose _NSGetArgv and _NSGetArgc as a nonempty list");

    const auto working_directory = syscape::process::working_directory();
    expect(working_directory && !working_directory->empty() &&
               working_directory->front() == '/',
           "macOS must report an absolute working directory");

    const auto cpu = syscape::process::cpu_time();
    expect(cpu && cpu->user >= std::chrono::nanoseconds::zero() &&
               cpu->system >= std::chrono::nanoseconds::zero(),
           "macOS must report nonnegative task execution times");

    const auto started = syscape::process::start_time();
    const std::chrono::system_clock::time_point year_2000(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::seconds(946684800)));
    expect(started && *started >= year_2000 &&
               *started <= std::chrono::system_clock::now(),
           "macOS must report a recorded process start instant");

    const auto memory = syscape::process::memory_usage();
    expect(memory && memory->resident_bytes > 0U &&
               memory->virtual_bytes > 0U,
           "macOS must report nonzero resident and virtual task sizes");

    const auto threads = syscape::process::thread_count();
    expect(threads && *threads >= 1U,
           "macOS must count at least the calling thread");

    return failures == 0 ? 0 : 1;
}
