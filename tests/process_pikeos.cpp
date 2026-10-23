#include <iostream>

#include <p4.h>

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
    expect(prio.has_value() && *prio == 120,
           "priority must match mock task priority (120) on PikeOS");

    syscape_test_set_pikeos_task_prio(64);
    const auto prio2 = syscape::process::priority();
    expect(prio2.has_value() && *prio2 == 64,
           "priority must dynamically reflect updated priority (64)");

    syscape_test_set_pikeos_task_null(1);
    const auto prio_null = syscape::process::priority();
    expect(!prio_null && prio_null.error() == syscape::errc::not_found,
           "priority must report not_found when p4_task_self() is 0");

    syscape_test_reset_pikeos_mock();

    const auto threads = syscape::process::thread_count();
    expect(!threads && threads.error() == syscape::errc::not_supported,
           "thread count must report not_supported on PikeOS");

    const auto pid = syscape::process::process_id();
    expect(!pid && pid.error() == syscape::errc::not_supported,
           "process id must report not_supported on PikeOS");

    const auto ppid = syscape::process::parent_process_id();
    expect(!ppid && ppid.error() == syscape::errc::not_supported,
           "parent process id must report not_supported on PikeOS");

    const auto exe = syscape::process::executable_path();
    expect(!exe && exe.error() == syscape::errc::not_supported,
           "executable path must report not_supported on PikeOS");

    const auto cmd = syscape::process::command_line();
    expect(!cmd && cmd.error() == syscape::errc::not_supported,
           "command line must report not_supported on PikeOS");

    const auto cwd = syscape::process::working_directory();
    expect(!cwd && cwd.error() == syscape::errc::not_supported,
           "working directory must report not_supported on PikeOS");
}

} // namespace

int main() {
    test_process_queries();
    return failures == 0 ? 0 : 1;
}
