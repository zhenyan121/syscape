#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

#include <sys/resource.h>
#include <unistd.h>

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

void test_scheduling_helpers() {
    using syscape::detail::process_backend::validate_priority;
    using syscape::detail::process_posix::convert_rlim_amount;
    using syscape::detail::process_posix::validate_limit_pair;
    using bound = syscape::detail::process_common::resource_limit_bound;

    const auto most_favorable = validate_priority(-20);
    expect(most_favorable && *most_favorable == -20,
           "The documented most favorable nice value must be accepted");

    const auto least_favorable = validate_priority(20);
    expect(least_favorable && *least_favorable == 20,
           "The Darwin-documented least favorable nice value must be "
           "accepted");

    const auto too_low = validate_priority(-21);
    expect(!too_low && too_low.error() == syscape::errc::malformed_data,
           "A nice value below the documented range must be malformed");

    const auto too_high = validate_priority(21);
    expect(!too_high && too_high.error() == syscape::errc::malformed_data,
           "A nice value above the documented range must be malformed");

    // A synthetic negative marker verifies that marker recognition precedes
    // rejection of other negative values. Darwin's real marker is positive.
    const auto signed_unlimited =
        convert_rlim_amount(static_cast<long long>(-1),
                            static_cast<long long>(-1));
    expect(signed_unlimited && signed_unlimited->unlimited,
           "A synthetic negative marker must record an unlimited bound");

    const auto signed_negative =
        convert_rlim_amount(static_cast<long long>(-5),
                            static_cast<long long>(-1));
    expect(!signed_negative &&
               signed_negative.error() == syscape::errc::malformed_data,
           "A non-marker negative amount must be malformed platform data");

    const auto signed_finite =
        convert_rlim_amount(static_cast<long long>(8192),
                            static_cast<long long>(-1));
    expect(signed_finite && !signed_finite->unlimited &&
               signed_finite->amount == 8192U,
           "A finite amount must convert unchanged");

    const syscape::result<void> ordered =
        validate_limit_pair(bound{100U, false}, bound{200U, false});
    expect(ordered.has_value(),
           "A soft bound within the hard bound is valid data");

    const syscape::result<void> soft_above_hard =
        validate_limit_pair(bound{300U, false}, bound{200U, false});
    expect(!soft_above_hard &&
               soft_above_hard.error() == syscape::errc::malformed_data,
           "A soft bound above a finite hard bound is malformed data");
}

} // namespace

int main() {
    test_conversion_helpers();
    test_scheduling_helpers();

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

    errno = 0;
    const int nice_reference = ::getpriority(::PRIO_PROCESS, 0);
    const auto scheduling = syscape::process::priority();
    if (!(nice_reference == -1 && errno != 0)) {
        expect(scheduling && *scheduling == nice_reference,
               "priority() must match an independent getpriority call");
        if (scheduling) {
            expect(*scheduling >= -20 && *scheduling <= 20,
                   "The reported priority must stay in the Darwin nice "
                   "range");
        }
    }

    const auto indices = syscape::process::cpu_affinity();
    expect(!indices &&
               indices.error() == std::errc::operation_not_supported,
           "macOS exposes no documented affinity source and must report "
           "the query as unsupported");

    const struct {
        syscape::process::resource_kind portable;
        int native;
    } cases[] = {
        {syscape::process::resource_kind::core_file_size, RLIMIT_CORE},
        {syscape::process::resource_kind::cpu_time, RLIMIT_CPU},
        {syscape::process::resource_kind::file_size, RLIMIT_FSIZE},
        {syscape::process::resource_kind::open_files, RLIMIT_NOFILE},
        {syscape::process::resource_kind::stack_size, RLIMIT_STACK},
        {syscape::process::resource_kind::address_space, RLIMIT_AS},
    };
    for (const auto& item : cases) {
        struct ::rlimit reference {};
        const int status = ::getrlimit(item.native, &reference);
        const auto limits =
            syscape::process::resource_limit(item.portable);
        if (status != 0) { continue; }
        expect(limits.has_value(),
               "resource_limit() must succeed where getrlimit succeeds");
        if (!limits) { continue; }
        const bool soft_matches =
            reference.rlim_cur == RLIM_INFINITY
                ? limits->soft.unlimited
                : !limits->soft.unlimited &&
                      limits->soft.amount ==
                          static_cast<std::uint64_t>(reference.rlim_cur);
        const bool hard_matches =
            reference.rlim_max == RLIM_INFINITY
                ? limits->hard.unlimited
                : !limits->hard.unlimited &&
                      limits->hard.amount ==
                          static_cast<std::uint64_t>(reference.rlim_max);
        expect(soft_matches,
               "The soft bound must match an independent getrlimit call");
        expect(hard_matches,
               "The hard bound must match an independent getrlimit call");
    }

    return failures == 0 ? 0 : 1;
}
