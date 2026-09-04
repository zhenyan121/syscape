#include <iostream>
#include <cstdlib>

#include <syscape/process.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_process_queries() {
    const auto pid = syscape::process::process_id();
    expect(pid && *pid > 0, "process id must be positive");

    const auto ppid = syscape::process::parent_process_id();
    expect(ppid.has_value(), "parent process id query must succeed");

    const auto threads = syscape::process::thread_count();
    expect(threads.error() == syscape::errc::not_supported,
           "thread count query must report not_supported on HP-UX");

    const auto mem = syscape::process::memory_usage();
    expect(mem.has_value(), "memory usage query must succeed");
    if (mem) {
        expect(mem->resident_bytes > 0, "resident memory must be positive");
        expect(mem->resident_bytes == 4997120ULL,
               "resident memory must include every resident segment");
        expect(mem->virtual_bytes >= mem->resident_bytes,
               "virtual memory must be >= resident memory");
        expect(mem->virtual_bytes == 12075008ULL,
               "virtual memory must accurately sum all segments");
    }

    const auto exe = syscape::process::executable_path();
    expect((exe && !exe->empty() && exe->front() == '/') ||
               exe.error() == syscape::errc::not_found ||
               exe.error() == syscape::errc::not_supported,
           "executable path must be an absolute path, not_found, or "
           "not_supported");

    const auto working_directory = syscape::process::working_directory();
    expect(working_directory && !working_directory->empty() &&
               working_directory->front() == '/',
           "working directory must be an absolute path");

    const auto cmdline = syscape::process::command_line();
    expect(cmdline.has_value() ||
               cmdline.error() == syscape::errc::not_supported ||
               cmdline.error() == syscape::errc::not_found,
           "command line query must succeed or report error");

    const auto cpu_time = syscape::process::cpu_time();
    expect(cpu_time && cpu_time->user.count() >= 0 &&
               cpu_time->system.count() >= 0,
           "CPU times must be nonnegative");

    const auto priority = syscape::process::priority();
    expect(priority.has_value(), "process priority query must succeed");

    const auto limits = syscape::process::resource_limit(
        syscape::process::resource_kind::open_files);
    expect(limits.has_value(), "open-file resource limit query must succeed");

#if defined(SYSCAPE_HPUX_PSTAT_MOCK)
    ::setenv("SYSCAPE_TEST_PSTAT_PROC_ZERO", "1", 1);
    expect(syscape::process::start_time().error() == syscape::errc::not_found,
           "an empty targeted process query must report not_found");
    expect(syscape::process::memory_usage().error() == syscape::errc::not_found,
           "empty targeted process memory must report not_found");
    ::unsetenv("SYSCAPE_TEST_PSTAT_PROC_ZERO");

    ::setenv("SYSCAPE_TEST_PSTAT_STATIC_ZERO", "1", 1);
    expect(syscape::process::memory_usage().error() ==
               syscape::errc::temporarily_unavailable,
           "an empty static snapshot must be temporarily unavailable");
    ::unsetenv("SYSCAPE_TEST_PSTAT_STATIC_ZERO");
#endif
}

} // namespace

int main() {
    test_process_queries();
    return failures == 0 ? 0 : 1;
}
