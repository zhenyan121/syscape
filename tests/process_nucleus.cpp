#include <iostream>

#include "nucleus.h"

#include <syscape/process.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

void test_process_queries() {
    const auto prio = syscape::process::priority();
    expect(prio.has_value() && *prio == 10,
           "priority must match mock task priority on Nucleus RTOS");

    syscape_test_set_nucleus_task_priority(25);
    const auto prio2 = syscape::process::priority();
    expect(prio2.has_value() && *prio2 == 25,
           "priority must dynamically reflect updated priority");

    syscape_test_set_nucleus_current_task_null(1);
    const auto prio_null = syscape::process::priority();
    expect(!prio_null && prio_null.error() == syscape::errc::not_found,
           "priority must report not_found when NU_Current_Task_Pointer() is "
           "nullptr");

    syscape_test_reset_nucleus_mock();

    const auto threads = syscape::process::thread_count();
    expect(!threads && threads.error() == syscape::errc::not_supported,
           "thread count must report not_supported on Nucleus RTOS");

    const auto pid = syscape::process::process_id();
    expect(!pid && pid.error() == syscape::errc::not_supported,
           "process id must report not_supported on Nucleus RTOS");

    const auto ppid = syscape::process::parent_process_id();
    expect(!ppid && ppid.error() == syscape::errc::not_supported,
           "parent process id must report not_supported on Nucleus RTOS");

    const auto exe = syscape::process::executable_path();
    expect(!exe && exe.error() == syscape::errc::not_supported,
           "executable path must report not_supported on Nucleus RTOS");

    const auto cmd = syscape::process::command_line();
    expect(!cmd && cmd.error() == syscape::errc::not_supported,
           "command line must report not_supported on Nucleus RTOS");

    const auto cwd = syscape::process::working_directory();
    expect(!cwd && cwd.error() == syscape::errc::not_supported,
           "working directory must report not_supported on Nucleus RTOS");
}

} // namespace

int main() {
    test_process_queries();
    return failures == 0 ? 0 : 1;
}
