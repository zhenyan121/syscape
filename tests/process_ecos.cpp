#include <iostream>

#include <cyg/kernel/kapi.h>

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
    // Mock base priority is 20, while current effective priority is 10.
    // syscape::process::priority() must report current priority (10).
    const auto prio = syscape::process::priority();
    expect(prio.has_value() && *prio == 10,
           "priority must match current effective priority (10) on eCos");

    // Changing base priority must not alter current effective priority.
    syscape_test_set_ecos_thread_priority(30);
    const auto prio_base = syscape::process::priority();
    expect(prio_base.has_value() && *prio_base == 10,
           "priority must still report current effective priority (10) when "
           "base changes");

    // Updating current effective priority must be dynamically reflected.
    syscape_test_set_ecos_thread_current_priority(5);
    const auto prio2 = syscape::process::priority();
    expect(prio2.has_value() && *prio2 == 5,
           "priority must dynamically reflect updated current priority");

    syscape_test_set_ecos_thread_null(1);
    const auto prio_null = syscape::process::priority();
    expect(!prio_null && prio_null.error() == syscape::errc::not_found,
           "priority must report not_found when cyg_thread_self() is 0");

    syscape_test_reset_ecos_mock();

    const auto threads = syscape::process::thread_count();
    expect(!threads && threads.error() == syscape::errc::not_supported,
           "thread count must report not_supported on eCos");

    const auto pid = syscape::process::process_id();
    expect(!pid && pid.error() == syscape::errc::not_supported,
           "process id must report not_supported on eCos");

    const auto ppid = syscape::process::parent_process_id();
    expect(!ppid && ppid.error() == syscape::errc::not_supported,
           "parent process id must report not_supported on eCos");

    const auto exe = syscape::process::executable_path();
    expect(!exe && exe.error() == syscape::errc::not_supported,
           "executable path must report not_supported on eCos");

    const auto cmd = syscape::process::command_line();
    expect(!cmd && cmd.error() == syscape::errc::not_supported,
           "command line must report not_supported on eCos");

    const auto cwd = syscape::process::working_directory();
    expect(!cwd && cwd.error() == syscape::errc::not_supported,
           "working directory must report not_supported on eCos");
}

} // namespace

int main() {
    test_process_queries();
    return failures == 0 ? 0 : 1;
}
