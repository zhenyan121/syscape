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
    expect(threads && *threads > 0, "thread count must be positive");

    const auto exe = syscape::process::executable_path();
    expect((exe && !exe->empty() && exe->front() == '/') ||
               exe.error() == syscape::errc::not_found,
           "executable path must be an absolute path or not_found");

    const auto working_directory = syscape::process::working_directory();
    expect(working_directory && !working_directory->empty() &&
               working_directory->front() == '/',
           "working directory must be an absolute path");

    const auto cmdline = syscape::process::command_line();
    expect(!cmdline && cmdline.error() == syscape::errc::not_supported,
           "command_line must report not_supported on Solaris");

    const auto cpu_time = syscape::process::cpu_time();
    expect(cpu_time && cpu_time->user.count() >= 0 &&
               cpu_time->system.count() >= 0,
           "CPU times must be nonnegative");

    const auto priority = syscape::process::priority();
    expect(priority.has_value(), "process priority query must succeed");

    const auto affinity = syscape::process::cpu_affinity();
    expect(!affinity && affinity.error() == syscape::errc::not_supported,
           "CPU affinity must report not_supported on Solaris");

    const auto limits = syscape::process::resource_limit(
        syscape::process::resource_kind::open_files);
    expect(limits.has_value(), "open-file resource limit query must succeed");
}

} // namespace

int main() {
    test_process_queries();
    return failures == 0 ? 0 : 1;
}
