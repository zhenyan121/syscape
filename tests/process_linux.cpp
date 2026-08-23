#include <cerrno>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

#include <syscape/detail/process/linux.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/os.hpp>
#include <syscape/process.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_command_line_parser() {
    constexpr std::string_view simple_input("one\0two\0", 8U);
    const auto simple =
        syscape::detail::process_backend::parse_command_line(simple_input);
    expect(simple && simple->size() == 2U && (*simple)[0] == "one" &&
               (*simple)[1] == "two",
           "NUL-separated command-line values must be preserved");

    constexpr std::string_view empty_value_input("first\0\0third\0", 13U);
    const auto empty_value =
        syscape::detail::process_backend::parse_command_line(
            empty_value_input);
    expect(empty_value && empty_value->size() == 3U &&
               (*empty_value)[1].empty(),
           "Empty command-line arguments are valid and must not be dropped");

    constexpr std::string_view one_empty_input("\0", 1U);
    const auto only_empty =
        syscape::detail::process_backend::parse_command_line(one_empty_input);
    expect(only_empty && only_empty->size() == 1U &&
               (*only_empty)[0].empty(),
           "An empty argv[0] is valid data");

    const auto empty_source =
        syscape::detail::process_backend::parse_command_line("");
    expect(empty_source && empty_source->empty(),
           "An empty command-line source represents an empty argument list");

    const auto missing_terminator =
        syscape::detail::process_backend::parse_command_line("broken");
    expect(!missing_terminator &&
               missing_terminator.error() ==
                   syscape::errc::malformed_data,
           "Command-line data without a final NUL must be malformed");

    const auto missing_last_terminator =
        syscape::detail::process_backend::parse_command_line("one\0two");
    expect(!missing_last_terminator &&
               missing_last_terminator.error() ==
                   syscape::errc::malformed_data,
           "The final command-line value still requires a terminating NUL");

    std::vector<std::string> invalid_encoding;
    invalid_encoding.emplace_back(1U, static_cast<char>(0xff));
    const auto invalid_text =
        syscape::detail::process_common::validate_utf8_arguments(
            syscape::result<std::vector<std::string>>(invalid_encoding));
    expect(!invalid_text &&
               invalid_text.error() == syscape::errc::invalid_encoding,
           "Invalid argument text must fail at the public boundary");
}

struct fake_read_link {
    std::string value;
    bool interrupted = true;

    ssize_t operator()(char* buffer, std::size_t size) {
        if (interrupted) {
            interrupted = false;
            errno = EINTR;
            return -1;
        }
        const std::size_t count = value.size() < size ? value.size() : size;
        value.copy(buffer, count);
        return static_cast<ssize_t>(count);
    }
};

void test_path_buffer_growth() {
    fake_read_link operation{"", false};
    // Start with enough bytes to force several doublings from the internal
    // 256-byte initial buffer without depending on PATH_MAX.
    constexpr std::size_t target_length = 700U;
    operation.value.resize(target_length, 'x');
    operation.value.front() = '/';

    const auto path = syscape::detail::process_backend::read_link_with_growth(
        operation);
    expect(path && path->size() == target_length,
           "Executable-path reads must grow truncated buffers");

    fake_read_link interrupted_operation{"/executable", true};
    const auto retried =
        syscape::detail::process_backend::read_link_with_growth(
            interrupted_operation);
    expect(retried && *retried == "/executable",
           "Interrupted executable-path reads must be retried");
}

void test_runtime_queries() {
    const auto id = syscape::process::process_id();
    expect(id && *id > 0U,
           "Linux must report a positive process ID from getpid()");

    const auto parent = syscape::process::parent_process_id();
    expect(parent.has_value(),
           "Linux must query the parent process ID from getppid()");

    const auto executable = syscape::process::executable_path();
    expect(executable && !executable->empty() &&
               executable->front() == '/' &&
               syscape::detail::is_valid_utf8(*executable),
           "Linux must report an absolute UTF-8 executable path from "
           "/proc/self/exe");

    const auto command_line = syscape::process::command_line();
    expect(command_line && !command_line->empty(),
           "Linux must report at least one command-line value");
    if (command_line) {
        for (const std::string& value : *command_line) {
            expect(syscape::detail::is_valid_utf8(value),
                   "Every command-line value must be valid UTF-8");
        }
    }

    const auto working_directory = syscape::process::working_directory();
    expect(working_directory && !working_directory->empty() &&
               working_directory->front() == '/' &&
               syscape::detail::is_valid_utf8(*working_directory),
           "Linux must report an absolute UTF-8 working directory from "
           "getcwd()");
}

std::string join_stat_line(std::string_view comm,
                           const std::vector<std::string>& fields) {
    std::string line("1234 (");
    line += comm;
    line += ")";
    for (const std::string& field : fields) {
        line += ' ';
        line += field;
    }
    return line;
}

std::vector<std::string> normal_stat_fields(std::uint64_t user_ticks,
                                            std::uint64_t system_ticks,
                                            std::uint64_t threads,
                                            std::uint64_t start_ticks,
                                            std::uint64_t virtual_size,
                                            std::uint64_t resident_pages) {
    // The documented post-name field order from state through rss.
    return {"S", "1", "1", "1", "0", "-1", "4194560", "100", "0", "0", "0",
            std::to_string(user_ticks), std::to_string(system_ticks), "0",
            "0", "20", "0", std::to_string(threads), "0",
            std::to_string(start_ticks), std::to_string(virtual_size),
            std::to_string(resident_pages)};
}

std::string make_stat_line(std::uint64_t user_ticks,
                           std::uint64_t system_ticks,
                           std::uint64_t threads,
                           std::uint64_t start_ticks,
                           std::uint64_t virtual_size,
                           std::uint64_t resident_pages,
                           std::string_view comm = "test proc (name)") {
    return join_stat_line(
        comm, normal_stat_fields(user_ticks, system_ticks, threads,
                                 start_ticks, virtual_size, resident_pages));
}

void test_stat_parser() {
    using syscape::detail::process_backend::parse_stat;

    const auto normal = parse_stat(make_stat_line(
        150U, 25U, 4U, 123456U, 409600U, 25U));
    expect(normal && normal->user_ticks == 150U &&
               normal->system_ticks == 25U && normal->threads == 4U &&
               normal->start_ticks == 123456U &&
               normal->virtual_size_bytes == 409600U &&
               normal->resident_pages == 25U,
           "A normal stat line must yield every documented field");

    const auto awkward_name = parse_stat(make_stat_line(
        1U, 2U, 1U, 3U, 4U, 5U, "sd-pam)) (weird name"));
    expect(awkward_name && awkward_name->user_ticks == 1U &&
               awkward_name->threads == 1U,
           "A process name containing parentheses and spaces must not "
           "shift the field boundary");

    const auto trailing_newline = parse_stat(make_stat_line(
        1U, 2U, 1U, 3U, 4U, 5U) + "\n");
    expect(trailing_newline && trailing_newline->resident_pages == 5U,
           "A trailing newline must not break the final field");

    const auto extra_fields = parse_stat(make_stat_line(
        1U, 2U, 1U, 3U, 4U, 5U) + " 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 "
        "0 0\n");
    expect(extra_fields && extra_fields->resident_pages == 5U,
           "Fields beyond the parsed range must be ignored");

    const auto empty_input = parse_stat("");
    expect(!empty_input && empty_input.error() ==
                               syscape::errc::malformed_data,
           "An empty stat source must be malformed platform data");

    const auto unclosed_name = parse_stat("1234 (broken S 1 1");
    expect(!unclosed_name && unclosed_name.error() ==
                                 syscape::errc::malformed_data,
           "A stat line without a closing parenthesis must be malformed");

    auto truncated_fields =
        normal_stat_fields(1U, 2U, 1U, 3U, 4U, 5U);
    truncated_fields.pop_back();
    const auto missing_final_field =
        parse_stat(join_stat_line("test", truncated_fields));
    expect(!missing_final_field && missing_final_field.error() ==
                                       syscape::errc::malformed_data,
           "A stat line missing the resident-pages field must be malformed");

    auto non_numeric_fields = normal_stat_fields(1U, 2U, 1U, 3U, 4U, 5U);
    non_numeric_fields.back() = "x";
    const auto non_numeric =
        parse_stat(join_stat_line("test", non_numeric_fields));
    expect(!non_numeric && non_numeric.error() ==
                               syscape::errc::malformed_data,
           "A non-numeric resident-pages field must be malformed");

    auto negative_fields = normal_stat_fields(1U, 2U, 1U, 3U, 4U, 5U);
    negative_fields.back() = "-5";
    const auto negative_pages =
        parse_stat(join_stat_line("test", negative_fields));
    expect(!negative_pages && negative_pages.error() ==
                                  syscape::errc::malformed_data,
           "A negative resident-pages field must be malformed");

    const auto zero_threads = parse_stat(make_stat_line(
        1U, 2U, 0U, 3U, 4U, 5U));
    expect(!zero_threads && zero_threads.error() ==
                                syscape::errc::malformed_data,
           "A live process cannot report zero threads");

    const auto oversized_threads = parse_stat(make_stat_line(
        1U, 2U, 5000000000ULL, 3U, 4U, 5U));
    expect(!oversized_threads && oversized_threads.error() ==
                                     syscape::errc::value_too_large,
           "A thread count beyond 32 bits must be reported as too large");

    auto huge_fields = normal_stat_fields(1U, 2U, 1U, 3U, 4U, 5U);
    huge_fields[19] = "99999999999999999999999999";
    const auto out_of_range_ticks =
        parse_stat(join_stat_line("test", huge_fields));
    expect(!out_of_range_ticks && out_of_range_ticks.error() ==
                                      syscape::errc::value_too_large,
           "A start offset beyond 64 bits must be reported as too large");
}

void test_tick_conversion() {
    using syscape::detail::process_backend::ticks_to_nanoseconds;

    const auto exact = ticks_to_nanoseconds(150U, 100L);
    expect(exact && exact->count() == 1500000000LL,
           "Whole-second tick amounts must convert exactly");

    const auto single_tick = ticks_to_nanoseconds(1U, 100L);
    expect(single_tick && single_tick->count() == 10000000LL,
           "One tick of a 100 Hz clock must convert to ten milliseconds");

    const auto zero_ticks = ticks_to_nanoseconds(0U, 100L);
    expect(zero_ticks && zero_ticks->count() == 0LL,
           "Zero ticks must convert to a zero duration");

    const auto fractional = ticks_to_nanoseconds(7U, 3L);
    expect(fractional && fractional->count() == 2333333333LL,
           "Fractional ticks must truncate toward zero without rounding");

    const auto zero_rate = ticks_to_nanoseconds(1U, 0L);
    const auto negative_rate = ticks_to_nanoseconds(1U, -100L);
    expect(!zero_rate && !negative_rate &&
               zero_rate.error() == syscape::errc::not_supported &&
               negative_rate.error() == syscape::errc::not_supported,
           "A nonpositive tick rate must report unusable granularity");

#if LONG_MAX > 18446744073LL
    const auto oversized_rate = ticks_to_nanoseconds(1U, 20000000000L);
    expect(!oversized_rate &&
               oversized_rate.error() == syscape::errc::not_supported,
           "A representable tick rate too large for exact arithmetic must "
           "report unusable granularity");
#endif

    const auto overflowing_ticks =
        ticks_to_nanoseconds(9223372037ULL * 100ULL, 100L);
    expect(!overflowing_ticks && overflowing_ticks.error() ==
                                     syscape::errc::value_too_large,
           "Tick amounts beyond the duration maximum must be reported as "
           "too large");
}

void test_memory_scaling_and_start_composition() {
    using syscape::detail::process_backend::scale_resident_bytes;
    using syscape::detail::process_backend::compose_start_time;

    const auto scaled = scale_resident_bytes(25U, 4096U);
    expect(scaled && *scaled == 102400U,
           "Resident pages must scale by the running page size");

    const auto zero_pages = scale_resident_bytes(0U, 4096U);
    expect(zero_pages && *zero_pages == 0U,
           "Zero resident pages are valid data");

    const auto zero_page_size = scale_resident_bytes(25U, 0U);
    expect(!zero_page_size && zero_page_size.error() ==
                                  syscape::errc::malformed_data,
           "A zero page size is malformed platform data");

    const auto overflowing_pages = scale_resident_bytes(
        (std::numeric_limits<std::uint64_t>::max)() / 4096U + 1U, 4096U);
    expect(!overflowing_pages && overflowing_pages.error() ==
                                     syscape::errc::value_too_large,
           "A resident product beyond 64 bits must be reported as too "
           "large");

    using clock = std::chrono::system_clock;
    const clock::time_point boot(clock::duration(1000));
    const auto composed = compose_start_time(
        boot, std::chrono::nanoseconds(500));
    expect(composed &&
               composed->time_since_epoch() == clock::duration(1500),
           "The start instant must add the process age to the boot time");

    const clock::time_point late_boot(
        std::chrono::duration_cast<clock::duration>(
            std::chrono::nanoseconds::max()));
    const auto unrepresentable =
        compose_start_time(late_boot, std::chrono::nanoseconds(1));
    expect(!unrepresentable && unrepresentable.error() ==
                                   syscape::errc::value_too_large,
           "A start instant beyond the clock range must be reported as "
           "too large");
}

void test_priority_validation() {
    using syscape::detail::process_backend::validate_priority;

    const auto most_favorable = validate_priority(-20);
    expect(most_favorable && *most_favorable == -20,
           "The documented most favorable nice value must be accepted");

    const auto normal = validate_priority(0);
    expect(normal && *normal == 0,
           "The normal nice value must be accepted unchanged");

    const auto least_favorable = validate_priority(19);
    expect(least_favorable && *least_favorable == 19,
           "The documented least favorable nice value must be accepted");

    const auto too_low = validate_priority(-21);
    expect(!too_low && too_low.error() == syscape::errc::malformed_data,
           "A nice value below the documented range must be malformed");

    const auto too_high = validate_priority(20);
    expect(!too_high && too_high.error() == syscape::errc::malformed_data,
           "A nice value above the documented range must be malformed");
}

void test_affinity_expansion() {
    using syscape::detail::process_backend::affinity_indices;

    unsigned long scattered[1] = {0b1010UL};
    const auto scattered_indices = affinity_indices(scattered, 1U);
    expect(scattered_indices && scattered_indices->size() == 2U &&
               (*scattered_indices)[0] == 1U && (*scattered_indices)[1] == 3U,
           "Set mask bits must map onto ascending processor indices");

    unsigned long second_word[2] = {0UL, 1UL};
    const auto crossed_word = affinity_indices(second_word, 2U);
    expect(crossed_word && crossed_word->size() == 1U &&
               (*crossed_word)[0] == 64U,
           "Bits in later words must continue the index sequence");

    const unsigned long all_bits[1] = {~0UL};
    const auto dense = affinity_indices(all_bits, 1U);
    expect(dense && dense->size() == sizeof(unsigned long) * 8U,
           "A fully set word must expand to every index in that word");

    const unsigned long empty[2] = {0UL, 0UL};
    const auto empty_mask = affinity_indices(empty, 2U);
    expect(!empty_mask &&
               empty_mask.error() == syscape::errc::malformed_data,
           "An empty affinity mask cannot describe a runnable process");
}

void test_limit_conversion_and_validation() {
    namespace posix = syscape::detail::process_posix;
    using bound = syscape::detail::process_common::resource_limit_bound;

    const ::rlim_t finite_marker = static_cast<::rlim_t>(4096);
    const auto finite =
        posix::convert_rlim_amount(finite_marker, RLIM_INFINITY);
    expect(finite && !finite->unlimited && finite->amount == 4096U,
           "A finite recorded limit must convert with its amount");

    const auto unlimited =
        posix::convert_rlim_amount(RLIM_INFINITY, RLIM_INFINITY);
    expect(unlimited && unlimited->unlimited,
           "The platform infinity marker must record an unlimited bound");

    const auto signed_unlimited =
        posix::convert_rlim_amount(static_cast<long long>(-1),
                                   static_cast<long long>(-1));
    expect(signed_unlimited && signed_unlimited->unlimited,
           "A signed limit type's negative infinity marker must record an "
           "unlimited bound");

    const auto signed_negative =
        posix::convert_rlim_amount(static_cast<long long>(-5),
                                   static_cast<long long>(-1));
    expect(!signed_negative &&
               signed_negative.error() == syscape::errc::malformed_data,
           "A signed limit type's other negative amounts must be malformed");

    const auto signed_finite =
        posix::convert_rlim_amount(static_cast<long long>(8192),
                                   static_cast<long long>(-1));
    expect(signed_finite && !signed_finite->unlimited &&
               signed_finite->amount == 8192U,
           "A signed limit type's finite amount must convert unchanged");

    const syscape::result<void> ordered = posix::validate_limit_pair(
        bound{100U, false}, bound{200U, false});
    expect(ordered.has_value(),
           "A soft bound within the hard bound is valid data");

    const syscape::result<void> equal = posix::validate_limit_pair(
        bound{200U, false}, bound{200U, false});
    expect(equal.has_value(), "Equal soft and hard bounds are valid data");

    const syscape::result<void> both_unlimited = posix::validate_limit_pair(
        bound{0U, true}, bound{0U, true});
    expect(both_unlimited.has_value(),
           "Unlimited soft and hard bounds are valid data");

    const syscape::result<void> finite_within_unlimited = posix::validate_limit_pair(
        bound{100U, false}, bound{0U, true});
    expect(finite_within_unlimited.has_value(),
           "A finite soft bound under an unlimited hard bound is valid "
           "data");

    const syscape::result<void> soft_above_hard = posix::validate_limit_pair(
        bound{300U, false}, bound{200U, false});
    expect(!soft_above_hard &&
               soft_above_hard.error() == syscape::errc::malformed_data,
           "A soft bound above a finite hard bound is malformed data");

    const syscape::result<void> unlimited_above_finite =
        posix::validate_limit_pair(bound{0U, true}, bound{50U, false});
    expect(!unlimited_above_finite &&
               unlimited_above_finite.error() ==
                   syscape::errc::malformed_data,
           "An unlimited soft bound above a finite hard bound is malformed "
           "data");
}

void test_scheduling_queries() {
    errno = 0;
    const int nice_reference = ::getpriority(::PRIO_PROCESS, 0);
    const bool nice_reference_failed =
        (nice_reference == -1 && errno != 0);

    const auto scheduling = syscape::process::priority();
    if (!nice_reference_failed) {
        expect(scheduling && *scheduling == nice_reference,
               "priority() must match an independent getpriority call");
        if (scheduling) {
            expect(*scheduling >= -20 && *scheduling <= 19,
                   "The reported priority must stay in the Linux nice "
                   "range");
        }
    }

    ::cpu_set_t reference_set {};
    CPU_ZERO(&reference_set);
    const int reference_status = ::sched_getaffinity(
        0, sizeof(reference_set), &reference_set);

    const auto indices = syscape::process::cpu_affinity();
    expect(indices && !indices->empty(),
           "Linux must expand a nonempty affinity mask");
    if (!indices) { return; }

    for (std::size_t position = 1U; position < indices->size();
         ++position) {
        expect((*indices)[position - 1U] < (*indices)[position],
               "Affinity indices must be strictly ascending without "
               "duplicates");
    }

    if (reference_status != 0) { return; }
    std::vector<std::uint32_t> reference_indices;
    for (std::size_t index = 0; index < static_cast<std::size_t>(CPU_SETSIZE);
         ++index) {
        if (CPU_ISSET(index, &reference_set)) {
            reference_indices.push_back(static_cast<std::uint32_t>(index));
        }
    }
    expect(*indices == reference_indices,
           "cpu_affinity() must match an independent sched_getaffinity "
           "call");
}

void test_resource_limit_queries() {
    struct limit_case {
        syscape::process::resource_kind portable;
        int native;
    };
    const limit_case cases[] = {
        {syscape::process::resource_kind::core_file_size, RLIMIT_CORE},
        {syscape::process::resource_kind::cpu_time, RLIMIT_CPU},
        {syscape::process::resource_kind::file_size, RLIMIT_FSIZE},
        {syscape::process::resource_kind::open_files, RLIMIT_NOFILE},
        {syscape::process::resource_kind::stack_size, RLIMIT_STACK},
        {syscape::process::resource_kind::address_space, RLIMIT_AS},
    };

    for (const limit_case& item : cases) {
        struct ::rlimit reference {};
        const int status = ::getrlimit(item.native, &reference);
        const auto limits = syscape::process::resource_limit(item.portable);
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

        const bool ordered =
            limits->hard.unlimited ||
            (!limits->soft.unlimited &&
             limits->soft.amount <= limits->hard.amount);
        expect(ordered,
               "The soft bound must never exceed the hard bound");
    }

    const auto open_files = syscape::process::resource_limit(
        syscape::process::resource_kind::open_files);
    expect(open_files && open_files->soft.amount > 0U,
           "Linux must record a positive open-files limit");

    const auto invalid = syscape::process::resource_limit(
        static_cast<syscape::process::resource_kind>(999));
    expect(!invalid && invalid.error() == syscape::errc::invalid_argument,
           "An unrecognized resource kind must not select a real limit");
}

void test_runtime_attribute_queries() {
    using namespace std::chrono_literals;

    const auto boot = syscape::os::boot_time();
    const auto now = std::chrono::system_clock::now();

    const auto start = syscape::process::start_time();
    expect(start && boot && *start >= *boot &&
               *start <= now + 1s,
           "Linux must derive a start instant between boot time and now");

    const auto cpu = syscape::process::cpu_time();
    expect(cpu && cpu->user >= 0ns && cpu->system >= 0ns,
           "Linux must report nonnegative user and system CPU times");
    if (cpu && start) {
        const auto age = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now - *start);
        const auto consumed = cpu->user + cpu->system;
        expect(consumed <= age + 1s,
               "Consumed CPU time must not exceed the process age");
    }

    const auto cpu_again = syscape::process::cpu_time();
    expect(cpu_again && cpu && cpu_again->user >= cpu->user &&
               cpu_again->system >= cpu->system,
           "CPU-time amounts must never decrease");

    const auto memory = syscape::process::memory_usage();
    expect(memory && memory->resident_bytes > 0U &&
               memory->virtual_bytes >= memory->resident_bytes,
           "Linux must report a nonzero resident set within the virtual "
           "address space");

    const auto threads = syscape::process::thread_count();
    expect(threads && *threads >= 1U,
           "Linux must count at least the calling thread");
}

} // namespace

int main() {
    test_command_line_parser();
    test_path_buffer_growth();
    test_stat_parser();
    test_tick_conversion();
    test_memory_scaling_and_start_composition();
    test_priority_validation();
    test_affinity_expansion();
    test_limit_conversion_and_validation();
    test_scheduling_queries();
    test_resource_limit_queries();
    test_runtime_queries();
    test_runtime_attribute_queries();
    return failures == 0 ? 0 : 1;
}
