#include <iostream>

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
    expect(!threads && threads.error() == syscape::errc::not_supported,
           "thread count query must report not_supported on VxWorks");

    const auto working_directory = syscape::process::working_directory();
    expect(working_directory && !working_directory->empty() &&
               working_directory->front() == '/',
           "working directory must be an absolute path");

    const auto exe = syscape::process::executable_path();
    expect((exe && !exe->empty() && exe->front() == '/') ||
               exe.error() == syscape::errc::not_found ||
               exe.error() == syscape::errc::not_supported,
           "executable path must be an absolute path, not_found, or "
           "not_supported");

    const auto cmdline = syscape::process::command_line();
    expect(cmdline.error() == syscape::errc::not_supported,
           "command line must report not_supported on VxWorks");

    const auto cpu_time = syscape::process::cpu_time();
    expect(cpu_time && cpu_time->user.count() >= 0 &&
               cpu_time->system.count() >= 0,
           "CPU times must be nonnegative");

    const auto mem = syscape::process::memory_usage();
    expect(mem.error() == syscape::errc::not_supported,
           "memory usage must report not_supported on VxWorks");

    const auto priority = syscape::process::priority();
    expect(priority.has_value() && *priority >= 0 && *priority <= 255,
           "process priority query must return a priority in range [0, 255]");

    const auto limits = syscape::process::resource_limit(
        syscape::process::resource_kind::open_files);
    expect(limits.has_value(), "open-file resource limit query must succeed");
}

void test_priority_validation() {
    auto p0 = syscape::detail::process_backend::validate_priority(0);
    expect(p0.has_value() && *p0 == 0, "priority 0 must be valid");

    auto p100 = syscape::detail::process_backend::validate_priority(100);
    expect(p100.has_value() && *p100 == 100, "priority 100 must be valid");

    auto p220 = syscape::detail::process_backend::validate_priority(220);
    expect(p220.has_value() && *p220 == 220, "priority 220 must be valid");

    auto p255 = syscape::detail::process_backend::validate_priority(255);
    expect(p255.has_value() && *p255 == 255, "priority 255 must be valid");

    auto p_neg = syscape::detail::process_backend::validate_priority(-1);
    expect(!p_neg && p_neg.error() == syscape::errc::malformed_data,
           "negative priority must fail with malformed_data");

    auto p_over = syscape::detail::process_backend::validate_priority(256);
    expect(!p_over && p_over.error() == syscape::errc::malformed_data,
           "priority > 255 must fail with malformed_data");
}

} // namespace

int main() {
    test_process_queries();
    test_priority_validation();
    return failures == 0 ? 0 : 1;
}
