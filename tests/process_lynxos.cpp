#include <iostream>

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
    const auto pid = syscape::process::process_id();
#if defined(SYSCAPE_LYNXOS_PID)
    expect(pid.has_value() && *pid == SYSCAPE_LYNXOS_PID,
           "process_id must match SYSCAPE_LYNXOS_PID");
#elif defined(LYNXOS_PID)
    expect(pid.has_value() && *pid == LYNXOS_PID,
           "process_id must match LYNXOS_PID");
#else
    expect(pid.has_value() && *pid > 0, "process_id must be positive");
#endif

    const auto ppid = syscape::process::parent_process_id();
#if defined(SYSCAPE_LYNXOS_PPID)
    expect(ppid.has_value() && *ppid == SYSCAPE_LYNXOS_PPID,
           "parent_process_id must match SYSCAPE_LYNXOS_PPID");
#elif defined(LYNXOS_PPID)
    expect(ppid.has_value() && *ppid == LYNXOS_PPID,
           "parent_process_id must match LYNXOS_PPID");
#else
    expect(ppid.has_value(), "parent_process_id must succeed");
#endif

    const auto exe = syscape::process::executable_path();
    expect(!exe && exe.error() == syscape::errc::not_supported,
           "executable_path must report not_supported on LynxOS");

    const auto cmd = syscape::process::command_line();
    expect(!cmd && cmd.error() == syscape::errc::not_supported,
           "command_line must report not_supported on LynxOS");

    const auto cwd = syscape::process::working_directory();
    expect(cwd.has_value() && !cwd->empty(),
           "working_directory must return non-empty path on LynxOS");

    const auto cpu = syscape::process::cpu_time();
    expect(!cpu && cpu.error() == syscape::errc::not_supported,
           "cpu_time must report not_supported on LynxOS");

    const auto start = syscape::process::start_time();
    expect(!start && start.error() == syscape::errc::not_supported,
           "start_time must report not_supported on LynxOS");

    const auto mem = syscape::process::memory_usage();
    expect(!mem && mem.error() == syscape::errc::not_supported,
           "memory_usage must report not_supported on LynxOS");

    const auto threads = syscape::process::thread_count();
    expect(!threads && threads.error() == syscape::errc::not_supported,
           "thread_count must report not_supported on LynxOS");

    const auto prio = syscape::process::priority();
#if defined(SYSCAPE_LYNXOS_PRIORITY)
    expect(prio.has_value() && *prio == SYSCAPE_LYNXOS_PRIORITY,
           "priority must match SYSCAPE_LYNXOS_PRIORITY");
#elif defined(LYNXOS_PRIORITY)
    expect(prio.has_value() && *prio == LYNXOS_PRIORITY,
           "priority must match LYNXOS_PRIORITY");
#else
    expect(prio.has_value(), "priority must succeed on LynxOS POSIX host");
#endif

    const auto affinity = syscape::process::cpu_affinity();
    expect(!affinity && affinity.error() == syscape::errc::not_supported,
           "cpu_affinity must report not_supported on LynxOS");

    const auto rlim = syscape::process::resource_limit(
        syscape::process::resource_kind::open_files);
    expect(!rlim && rlim.error() == syscape::errc::not_supported,
           "resource_limit must report not_supported on LynxOS");
}

} // namespace

int main() {
    test_process_queries();
    return failures == 0 ? 0 : 1;
}
