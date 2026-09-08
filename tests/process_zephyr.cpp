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
    expect(!pid && pid.error() == syscape::errc::not_supported,
           "process id query must report not_supported on Zephyr");

    const auto ppid = syscape::process::parent_process_id();
    expect(!ppid && ppid.error() == syscape::errc::not_supported,
           "parent process id query must report not_supported on Zephyr");

    const auto threads = syscape::process::thread_count();
    expect(!threads && threads.error() == syscape::errc::not_supported,
           "thread count query must report not_supported on Zephyr");

    const auto working_directory = syscape::process::working_directory();
    expect(!working_directory &&
               working_directory.error() == syscape::errc::not_supported,
           "working directory must report not_supported on Zephyr");

    const auto exe = syscape::process::executable_path();
    expect(!exe && exe.error() == syscape::errc::not_supported,
           "executable path must report not_supported on Zephyr");

    const auto cmdline = syscape::process::command_line();
    expect(cmdline.error() == syscape::errc::not_supported,
           "command line must report not_supported on Zephyr");

    const auto cpu_time = syscape::process::cpu_time();
    expect(!cpu_time && cpu_time.error() == syscape::errc::not_supported,
           "CPU times must report not_supported on Zephyr");

    const auto mem = syscape::process::memory_usage();
    expect(mem.error() == syscape::errc::not_supported,
           "memory usage must report not_supported on Zephyr");

    const auto priority = syscape::process::priority();
    expect(!priority && priority.error() == syscape::errc::not_supported,
           "process priority query must report not_supported on Zephyr");

    const auto limits = syscape::process::resource_limit(
        syscape::process::resource_kind::open_files);
    expect(!limits && limits.error() == syscape::errc::not_supported,
           "resource limits must report not_supported on Zephyr");
}

} // namespace

int main() {
    test_process_queries();
    return failures == 0 ? 0 : 1;
}
