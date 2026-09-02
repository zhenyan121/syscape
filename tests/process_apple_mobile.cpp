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
    expect(pid && *pid > 0U, "process ID must be positive");

    const auto ppid = syscape::process::parent_process_id();
    expect(ppid.has_value(), "parent process ID query must succeed");

    const auto exe = syscape::process::executable_path();
    expect(exe.has_value() || exe.error() == syscape::errc::not_supported ||
               exe.error() == syscape::errc::permission_denied,
           "executable path query must succeed or report expected error");

    const auto cwd = syscape::process::working_directory();
    expect(cwd && !cwd->empty(), "working directory must be nonempty");

    const auto threads = syscape::process::thread_count();
    expect(threads && *threads > 0U, "thread count must be positive");

    const auto cpu = syscape::process::cpu_time();
    expect(cpu.has_value() || cpu.error() == syscape::errc::not_supported,
           "cpu time query must succeed or report not_supported");

    const auto mem = syscape::process::memory_usage();
    expect(mem.has_value() || mem.error() == syscape::errc::not_supported,
           "memory usage query must succeed or report not_supported");

    const auto start = syscape::process::start_time();
    expect(start.has_value() || start.error() == syscape::errc::not_supported,
           "start time query must succeed or report not_supported");

    const auto prio = syscape::process::priority();
    expect(prio.has_value() || prio.error() == syscape::errc::not_supported,
           "priority query must succeed or report not_supported");

    const auto aff = syscape::process::cpu_affinity();
    expect(aff.has_value() || aff.error() == syscape::errc::not_supported,
           "cpu affinity query must succeed or report not_supported");

    const auto lim = syscape::process::resource_limit(
        syscape::process::limit_resource::open_files);
    expect(lim.has_value() || lim.error() == syscape::errc::not_supported,
           "resource limit query must succeed or report not_supported");
}

} // namespace

int main() {
    test_process_queries();
    return failures == 0 ? 0 : 1;
}
