#include <chrono>
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
    expect((threads && *threads > 0) ||
               threads.error() == syscape::errc::not_supported,
           "thread count must be positive or not_supported");
    const auto wd = syscape::process::working_directory();
    expect(wd && !wd->empty() && wd->front() == '/',
           "working directory must be an absolute path");
    const auto exe = syscape::process::executable_path();
    expect((exe && !exe->empty() && exe->front() == '/') ||
               exe.error() == syscape::errc::not_supported,
           "executable path must be an absolute path or not_supported");
    const auto cmdline = syscape::process::command_line();
    expect(!cmdline && cmdline.error() == syscape::errc::not_supported,
           "command line query must report not_supported on Haiku");
    const auto time = syscape::process::cpu_time();
    expect(time.has_value() || time.error() == syscape::errc::not_supported,
           "cpu time must succeed or report not_supported");
    const auto mem = syscape::process::memory_usage();
    expect(mem.has_value() || mem.error() == syscape::errc::not_supported,
           "memory usage must succeed or report not_supported");
    const auto st = syscape::process::start_time();
    if (st) {
        const auto epoch_s = std::chrono::duration_cast<std::chrono::seconds>(
                                 st->time_since_epoch())
                                 .count();
        expect(epoch_s > 1577836800LL,
               "start_time must be realistic wall-clock timestamp");
    }
}

} // namespace

int main() {
    test_process_queries();
    return failures == 0 ? 0 : 1;
}
