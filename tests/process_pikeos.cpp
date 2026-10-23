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
#if defined(SYSCAPE_PIKEOS_PRIORITY)
    expect(prio.has_value() && *prio == SYSCAPE_PIKEOS_PRIORITY,
           "priority must report SYSCAPE_PIKEOS_PRIORITY");
#elif defined(PIKEOS_PRIORITY)
    expect(prio.has_value() && *prio == PIKEOS_PRIORITY,
           "priority must report PIKEOS_PRIORITY");
#else
    expect(!prio && prio.error() == syscape::errc::not_supported,
           "priority must report not_supported on PikeOS by default");
#endif

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
