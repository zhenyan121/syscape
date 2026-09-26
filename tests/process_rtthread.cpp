#include <iostream>

#include "rtdef.h"
#include "rtthread.h"

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
    const auto prio = syscape::process::priority();
    expect(prio.has_value() && *prio == 12,
           "priority must match mock thread priority on RT-Thread");

    rtthread_mock_set_priority(5);
    const auto prio2 = syscape::process::priority();
    expect(prio2.has_value() && *prio2 == 5,
           "priority must dynamically reflect updated priority");

    rtthread_mock_set_thread(nullptr);
    const auto prio_null = syscape::process::priority();
    expect(!prio_null && prio_null.error() == syscape::errc::not_found,
           "priority must report not_found when rt_thread_self is null");

    rtthread_mock_reset();

    const auto threads = syscape::process::thread_count();
    expect(threads.has_value() && *threads == RT_THREAD_PRIORITY_MAX,
           "thread count must report RT_THREAD_PRIORITY_MAX");

    const auto pid = syscape::process::process_id();
    expect(!pid && pid.error() == syscape::errc::not_supported,
           "process id must report not_supported on RT-Thread");

    const auto ppid = syscape::process::parent_process_id();
    expect(!ppid && ppid.error() == syscape::errc::not_supported,
           "parent process id must report not_supported on RT-Thread");

    const auto exe = syscape::process::executable_path();
    expect(!exe && exe.error() == syscape::errc::not_supported,
           "executable path must report not_supported on RT-Thread");

    const auto cmd = syscape::process::command_line();
    expect(!cmd && cmd.error() == syscape::errc::not_supported,
           "command line must report not_supported on RT-Thread");

    const auto cwd = syscape::process::working_directory();
    expect(!cwd && cwd.error() == syscape::errc::not_supported,
           "working directory must report not_supported on RT-Thread");

    const auto cpu = syscape::process::cpu_time();
    expect(!cpu && cpu.error() == syscape::errc::not_supported,
           "cpu time must report not_supported on RT-Thread");

    const auto start = syscape::process::start_time();
    expect(!start && start.error() == syscape::errc::not_supported,
           "start time must report not_supported on RT-Thread");

    const auto mem = syscape::process::memory_usage();
    expect(!mem && mem.error() == syscape::errc::not_supported,
           "memory usage must report not_supported on RT-Thread");

    const auto aff = syscape::process::cpu_affinity();
    expect(!aff && aff.error() == syscape::errc::not_supported,
           "cpu affinity must report not_supported on RT-Thread");

    const auto limit = syscape::process::resource_limit(
        syscape::process::resource_kind::open_files);
    expect(!limit && limit.error() == syscape::errc::not_supported,
           "resource limit must report not_supported on RT-Thread");
}

} // namespace

int main() {
    test_process_queries();
    return failures == 0 ? 0 : 1;
}
