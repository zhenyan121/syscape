#include <iostream>

#include "thread.h"

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
    expect(prio.has_value() && *prio == 5,
           "priority must match mock thread priority on RIOT OS");

    riot_mock_set_priority(12);
    const auto prio2 = syscape::process::priority();
    expect(prio2.has_value() && *prio2 == 12,
           "priority must dynamically reflect updated priority");
    riot_mock_set_priority(5);

    riot_mock_set_current_pid(KERNEL_PID_UNDEF);
    const auto prio_not_found = syscape::process::priority();
    expect(!prio_not_found &&
               prio_not_found.error() == syscape::errc::not_found,
           "priority must return not_found when active thread is not in table");
    riot_mock_set_current_pid(1);

    const auto pid = syscape::process::process_id();
    expect(!pid && pid.error() == syscape::errc::not_supported,
           "process id must report not_supported on RIOT OS");

    const auto ppid = syscape::process::parent_process_id();
    expect(!ppid && ppid.error() == syscape::errc::not_supported,
           "parent process id must report not_supported on RIOT OS");

    const auto exe = syscape::process::executable_path();
    expect(!exe && exe.error() == syscape::errc::not_supported,
           "executable path must report not_supported on RIOT OS");

    const auto cmd = syscape::process::command_line();
    expect(!cmd && cmd.error() == syscape::errc::not_supported,
           "command line must report not_supported on RIOT OS");

    const auto cwd = syscape::process::working_directory();
    expect(!cwd && cwd.error() == syscape::errc::not_supported,
           "working directory must report not_supported on RIOT OS");

    const auto cpu = syscape::process::cpu_time();
    expect(!cpu && cpu.error() == syscape::errc::not_supported,
           "cpu time must report not_supported on RIOT OS");

    const auto start = syscape::process::start_time();
    expect(!start && start.error() == syscape::errc::not_supported,
           "start time must report not_supported on RIOT OS");

    const auto mem = syscape::process::memory_usage();
    expect(!mem && mem.error() == syscape::errc::not_supported,
           "memory usage must report not_supported on RIOT OS");

    const auto threads = syscape::process::thread_count();
    expect(threads.has_value() && *threads == 4U,
           "thread count must match mock value on RIOT OS");

    riot_mock_set_thread_count(7);
    const auto threads2 = syscape::process::thread_count();
    expect(threads2.has_value() && *threads2 == 7U,
           "thread count must dynamically reflect updated mock value");
    riot_mock_set_thread_count(-1);
    const auto threads_neg = syscape::process::thread_count();
    expect(!threads_neg && threads_neg.error() == syscape::errc::malformed_data,
           "thread count must report malformed_data when sched_num_threads is "
           "negative");
    riot_mock_set_thread_count(4);

    const auto aff = syscape::process::cpu_affinity();
    expect(!aff && aff.error() == syscape::errc::not_supported,
           "cpu affinity must report not_supported on RIOT OS");

    const auto lim = syscape::process::resource_limit(
        syscape::process::resource_kind::open_files);
    expect(!lim && lim.error() == syscape::errc::not_supported,
           "resource limit must report not_supported on RIOT OS");
}

} // namespace

int main() {
    test_process_queries();
    return failures == 0 ? 0 : 1;
}
