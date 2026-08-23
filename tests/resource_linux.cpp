#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <system_error>

#include <syscape/detail/resource/common.hpp>
#include <syscape/detail/resource/linux.hpp>
#include <syscape/resource.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_load_sample_parser() {
    namespace backend = syscape::detail::resource_backend;

    const auto plain = backend::parse_load_sample("0.52");
    expect(plain && std::fabs(*plain - 0.52) < 1e-9,
           "A kernel-rendered sample must parse to its exact value");

    const auto whole = backend::parse_load_sample("12.00");
    expect(whole && *whole == 12.0,
           "A whole-number sample must parse exactly");

    const auto idle = backend::parse_load_sample("0.00");
    expect(idle && *idle == 0.0, "Zero load is valid data for an idle system");

    const auto long_fraction = backend::parse_load_sample("1.234567890");
    expect(long_fraction && std::fabs(*long_fraction - 1.23456789) < 1e-9,
           "Multi-digit fractions must parse by their digit count");

    const auto missing_point = backend::parse_load_sample("100");
    expect(!missing_point &&
               missing_point.error() == syscape::errc::malformed_data,
           "A sample without a decimal point must be malformed");

    const auto leading_point = backend::parse_load_sample(".52");
    expect(!leading_point &&
               leading_point.error() == syscape::errc::malformed_data,
           "A sample without integer digits must be malformed");

    const auto trailing_point = backend::parse_load_sample("0.");
    expect(!trailing_point &&
               trailing_point.error() == syscape::errc::malformed_data,
           "A sample without fraction digits must be malformed");

    const auto second_point = backend::parse_load_sample("1.2.3");
    expect(!second_point &&
               second_point.error() == syscape::errc::malformed_data,
           "A sample with two decimal points must be malformed");

    const auto negative = backend::parse_load_sample("-1.00");
    expect(!negative && negative.error() == syscape::errc::malformed_data,
           "A negative load sample cannot come from the documented format");

    const auto nonnumeric = backend::parse_load_sample("abc.def");
    expect(!nonnumeric &&
               nonnumeric.error() == syscape::errc::malformed_data,
           "A nonnumeric sample must be malformed");

    const auto oversized = backend::parse_load_sample("12345678901.00");
    expect(!oversized && oversized.error() == syscape::errc::malformed_data,
           "An implausibly long rendering must be rejected");
}

void test_loadavg_parser() {
    namespace backend = syscape::detail::resource_backend;

    const auto full = backend::parse_loadavg(
        "0.52 0.58 0.59 1/456 12345\n");
    expect(full.has_value(), "The documented five fields must parse");
    expect(full && std::fabs(full->loads.one_minute - 0.52) < 1e-9 &&
               std::fabs(full->loads.five_minute - 0.58) < 1e-9 &&
               std::fabs(full->loads.fifteen_minute - 0.59) < 1e-9,
           "The three samples must map to one, five, and fifteen minutes");
    expect(full && full->entities.runnable == 1U &&
               full->entities.schedulable == 456U,
           "The running/total pair must map to the entity counts");

    const auto idle_no_newline = backend::parse_loadavg(
        "0.00 0.00 0.00 0/12 999");
    expect(idle_no_newline && idle_no_newline->entities.runnable == 0U &&
               idle_no_newline->entities.schedulable == 12U,
           "Zero entities are valid data and the last line needs no newline");

    const auto busy = backend::parse_loadavg(
        "120.50 118.25 90.10 300/4000 777777\n");
    expect(busy && busy->entities.runnable == 300U,
           "Large entity counts must survive parsing");

    const auto mixed_whitespace =
        backend::parse_loadavg("0.10  0.20\t0.30 2/50 77");
    expect(mixed_whitespace &&
               std::fabs(mixed_whitespace->loads.fifteen_minute - 0.30) <
                   1e-9 &&
               mixed_whitespace->entities.runnable == 2U,
           "Space and tab separators must both tokenize");

    const auto three_fields =
        backend::parse_loadavg("0.52 0.58 0.59\n");
    expect(!three_fields &&
               three_fields.error() == syscape::errc::malformed_data,
           "Missing entity and identifier fields must be malformed");

    const auto bad_entities =
        backend::parse_loadavg("0.52 0.58 0.59 1456 12345\n");
    expect(!bad_entities &&
               bad_entities.error() == syscape::errc::malformed_data,
           "An entity field without its slash must be malformed");

    const auto empty_runnable =
        backend::parse_loadavg("0.52 0.58 0.59 /456 12345\n");
    expect(!empty_runnable &&
               empty_runnable.error() == syscape::errc::malformed_data,
           "An empty runnable count must be malformed");

    const auto nonnumeric_pid =
        backend::parse_loadavg("0.52 0.58 0.59 1/456 abc\n");
    expect(!nonnumeric_pid &&
               nonnumeric_pid.error() == syscape::errc::malformed_data,
           "A nonnumeric final identifier must be malformed");

    const auto trailing_junk =
        backend::parse_loadavg("0.52 0.58 0.59 1/456 12345 junk\n");
    expect(!trailing_junk &&
               trailing_junk.error() == syscape::errc::malformed_data,
           "Content beyond the documented five fields must be malformed");

    const auto overflow =
        backend::parse_loadavg(
            "0.52 0.58 0.59 1/99999999999999999999 12345\n");
    expect(!overflow && overflow.error() == syscape::errc::value_too_large,
           "Entity counts beyond 64 bits must report value_too_large");
}

void test_file_nr_parser() {
    namespace backend = syscape::detail::resource_backend;

    const auto normal = backend::parse_file_nr("3776 0 1631999\n");
    expect(normal && normal->allocated == 3776U &&
               normal->maximum == 1631999U,
           "Allocated and maximum handles must come from the documented "
           "positions");

    const auto zero_allocated = backend::parse_file_nr("0 0 1631999\n");
    expect(zero_allocated && zero_allocated->allocated == 0U,
           "Zero allocated handles are valid data, not an error sentinel");

    const auto no_newline = backend::parse_file_nr("10 20 30");
    expect(no_newline && no_newline->allocated == 10U &&
               no_newline->maximum == 30U,
           "The legacy free-count field is skipped and the last line may "
           "lack a newline");

    const auto two_fields = backend::parse_file_nr("3776 1631999\n");
    expect(!two_fields &&
               two_fields.error() == syscape::errc::malformed_data,
           "A record with fewer than three values must be malformed");

    const auto four_fields = backend::parse_file_nr("1 2 3 4\n");
    expect(!four_fields &&
               four_fields.error() == syscape::errc::malformed_data,
           "A record with more than three values must be malformed");

    const auto nonnumeric = backend::parse_file_nr("x 0 y\n");
    expect(!nonnumeric &&
               nonnumeric.error() == syscape::errc::malformed_data,
           "Nonnumeric file-table values must be malformed");

    const auto tab_separated =
        backend::parse_file_nr("9311\t0\t9223372036854775807\n");
    expect(tab_separated && tab_separated->allocated == 9311U &&
               tab_separated->maximum == 9223372036854775807ULL,
           "The kernel's sysctl rendering separates values with tabs");
}

void test_thread_count_parser() {
    namespace backend = syscape::detail::resource_backend;

    // A realistic record: pid (comm) state ppid pgrp session tty tpgid
    // flags minflt cminflt majflt cmajflt utime stime cutime cstime
    // priority nice num_threads itrealvalue starttime ...
    const std::string plain =
        "1847 (systemd-journal) S 1 1847 1847 0 -1 4194560 "
        "1234 5678 0 0 12 34 5 6789 0 20 4 0 12345 123456789 42 42 "
        "1847654321 1847654321 0 0 0 0 0 0 0 1847654321 0\n";
    const auto parsed = backend::parse_thread_count(plain);
    expect(parsed && *parsed == 4U,
           "num_threads must be extracted from its documented position");

    const std::string tricky_name =
        "99 ((we)ird name)) R 1 99 99 0 -1 4194304 0 0 0 0 0 0 0 0 "
        "20 0 7 0 1 55 55 1 1 0 0 0 0 0 0 0 1 0\n";
    const auto nested_parens = backend::parse_thread_count(tricky_name);
    expect(nested_parens && *nested_parens == 7U,
           "A process name containing parentheses must not shift fields");

    const auto minimum_fields =
        backend::parse_thread_count("5 (tiny) S 0 5 5 0 -1 0 0 0 0 0 0 0 "
                                    "0 0 20 0 3\n");
    expect(minimum_fields && *minimum_fields == 3U,
           "A record ending at num_threads must still parse");

    const auto truncated =
        backend::parse_thread_count("5 (short) S 0 5 5 0 -1 0 0 0 0\n");
    expect(!truncated &&
               truncated.error() == syscape::errc::malformed_data,
           "A record without the num_threads field must be malformed");

    const auto zero_threads =
        backend::parse_thread_count(
            "6 (ghost) S 0 6 6 0 -1 0 0 0 0 0 0 0 0 0 20 0 0\n");
    expect(!zero_threads &&
               zero_threads.error() == syscape::errc::malformed_data,
           "A live process always owns at least one thread");

    const auto nonnumeric =
        backend::parse_thread_count(
            "7 (bad) S 0 7 7 0 -1 0 0 0 0 0 0 0 0 0 20 0 many\n");
    expect(!nonnumeric && nonnumeric.error() == syscape::errc::malformed_data,
           "A nonnumeric thread count must be malformed");

    const auto no_name =
        backend::parse_thread_count("8 noparen S 0 8 8 0 -1 0 0 0 0 0 0 0 "
                                    "0 0 0 20 0 1\n");
    expect(!no_name && no_name.error() == syscape::errc::malformed_data,
           "A record without a parenthesized name must be malformed");
}

void test_common_validators() {
    namespace common = syscape::detail::resource_common;
    using syscape::fail;
    using syscape::result;
    using syscape::errc;

    const auto rejected_negative =
        common::validate_load_samples(result<common::load_samples>(
            common::load_samples{-1.0, 0.0, 0.0}));
    expect(!rejected_negative &&
               rejected_negative.error() == errc::malformed_data,
           "A negative load sample must be malformed platform data");

    const double nan_value = std::nan("");
    const auto rejected_nan =
        common::validate_load_samples(result<common::load_samples>(
            common::load_samples{nan_value, 0.0, 0.0}));
    expect(!rejected_nan && rejected_nan.error() == errc::malformed_data,
           "A non-finite load sample must be malformed platform data");

    const auto accepted_zero =
        common::validate_load_samples(result<common::load_samples>(
            common::load_samples{0.0, 0.0, 0.0}));
    expect(accepted_zero.has_value(),
           "All-zero samples are valid data for an idle system");

    const auto forwarded_error =
        common::validate_load_samples(result<common::load_samples>(
            fail(errc::not_supported)));
    expect(!forwarded_error &&
               forwarded_error.error() == errc::not_supported,
           "Validators must forward unsupported-capability errors unchanged");

    const auto rejected_ordering =
        common::validate_entity_counts(result<common::entity_counts>(
            common::entity_counts{5U, 4U}));
    expect(!rejected_ordering &&
               rejected_ordering.error() == errc::malformed_data,
           "Runnable entities exceeding the population must be malformed");

    const auto equal_ordering =
        common::validate_entity_counts(result<common::entity_counts>(
            common::entity_counts{4U, 4U}));
    expect(equal_ordering.has_value(),
           "Fully runnable populations are valid data");

    const auto zero_processes =
        common::validate_positive_count(result<std::uint64_t>(0U));
    expect(!zero_processes &&
               zero_processes.error() == errc::malformed_data,
           "A zero process or thread count cannot describe a live system");

    const auto one_process =
        common::validate_positive_count(result<std::uint64_t>(1U));
    expect(one_process.has_value(),
           "A positive count passes the liveness validation");
}

void test_runtime_queries() {
    using syscape::resource::file_descriptor_limit;
    using syscape::resource::load_average;
    using syscape::resource::open_file_count;
    using syscape::resource::process_count;
    using syscape::resource::scheduler_entities;
    using syscape::resource::thread_count;

    const auto loads = load_average();
    expect(loads.has_value(),
           "Linux must read the documented /proc/loadavg source");
    if (loads) {
        expect(loads->one_minute >= 0.0 && loads->five_minute >= 0.0 &&
                   loads->fifteen_minute >= 0.0 &&
                   std::isfinite(loads->one_minute) &&
                   std::isfinite(loads->five_minute) &&
                   std::isfinite(loads->fifteen_minute),
               "Load samples must be finite and nonnegative");
    }

    // Cross-check against an independent getloadavg reading of the same
    // kernel counters; the damped averages move slowly between calls.
    double reference[3] = {0.0, 0.0, 0.0};
    if (loads && ::getloadavg(reference, 3) == 3) {
        expect(std::fabs(loads->one_minute - reference[0]) < 0.5 &&
                   std::fabs(loads->five_minute - reference[1]) < 0.5 &&
                   std::fabs(loads->fifteen_minute - reference[2]) < 0.5,
               "Parsed samples must match an independent getloadavg read");
    }

    const auto entities = scheduler_entities();
    expect(entities.has_value(),
           "Linux must expose scheduler entities from /proc/loadavg");
    if (entities) {
        expect(entities->runnable_entities <= entities->total_entities,
               "Runnable entities can never exceed the total population");
    }

    const auto processes = process_count();
    expect(processes && *processes > 0U,
           "Linux must enumerate at least the calling process in /proc");

    // A visitor that always fails must abort and propagate its error, so a
    // walk can never report a silently incomplete result.
    const auto aborted =
        syscape::detail::resource_backend::walk_processes(
            [](const char*) -> syscape::result<void> {
                return syscape::fail(syscape::errc::permission_denied);
            });
    expect(!aborted &&
               aborted.error() == syscape::errc::permission_denied,
           "A failing visitor must propagate its error out of the walk");

    const auto completed =
        syscape::detail::resource_backend::walk_processes(
            [](const char*) { return syscape::result<void>{}; });
    expect(completed && *completed > 0U,
           "A succeeding visitor must complete the walk over every entry");

    // The real thread query must succeed on this documented source instead
    // of having its failure excused.
    const auto threads = thread_count();
    expect(threads.has_value(),
           "Linux must sum readable stat records into a thread total");
    if (threads) {
        expect(*threads > 0U,
               "Linux must sum at least the calling thread's record");
    }

    // Cross-check the per-record parser against this process's own live
    // stat record.
    if (threads) {
        const auto self_stat =
            syscape::detail::linux_platform::read_text_file("/proc/self/stat");
        const auto self_threads =
            self_stat ? syscape::detail::resource_backend::parse_thread_count(
                            *self_stat)
                      : syscape::result<std::uint64_t>(
                            syscape::fail(syscape::errc::not_found));
        if (self_threads) {
            expect(*self_threads <= *threads,
                   "This process's own thread count fits within the total");
        }
    }

    const auto open_files = open_file_count();
    expect(open_files.has_value(),
           "Linux must read the kernel-documented file-nr usage value");

    const auto limit = file_descriptor_limit();
    expect(limit && *limit > 0U,
           "Linux must report a positive system-wide handle maximum");
}

} // namespace

int main() {
    test_load_sample_parser();
    test_loadavg_parser();
    test_file_nr_parser();
    test_thread_count_parser();
    test_common_validators();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
