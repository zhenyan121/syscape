#include <cstdlib>
#include <iostream>

#include <syscape/process.hpp>
#include <syscape/process_list.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_process_list_queries() {
    const auto count = syscape::process_list::process_count();
    expect(count.has_value() || count.error() == syscape::errc::not_supported,
           "process count query must succeed or report not_supported");

    const auto procs = syscape::process_list::processes();
    expect(procs.has_value(), "processes query must succeed with pstat");
    if (procs) {
        expect(!procs->empty(), "processes list must not be empty");
        bool found_pid_zero = false;
        for (const auto& p : *procs) {
            found_pid_zero = found_pid_zero || p.pid == 0U;
            expect(p.uid.has_value(), "UID must be populated on HP-UX");
            expect(p.gid.has_value(), "GID must be populated on HP-UX");
            expect(!p.thread_count.has_value(),
                   "thread count must be nullopt on HP-UX");
            if (p.resident_memory_bytes && p.virtual_memory_bytes) {
                expect(*p.resident_memory_bytes > 0,
                       "resident memory must be positive");
                expect(*p.virtual_memory_bytes >= *p.resident_memory_bytes,
                       "virtual memory must be >= resident memory");
            }
        }
        expect(found_pid_zero,
               "process enumeration must preserve the HP-UX swapper PID 0");
        for (std::size_t i = 1; i < procs->size(); ++i) {
            expect(
                (*procs)[i - 1].pid < (*procs)[i].pid,
                "processes must be sorted in natural ascending order by PID");
        }
    }

    const auto my_pid = syscape::process::process_id();
    if (my_pid) {
        const auto self_proc = syscape::process_list::find_process(*my_pid);
        expect(self_proc.has_value(),
               "find_process for current process must succeed with pstat");
        if (self_proc) {
            expect(self_proc->pid == *my_pid, "PID must match current process");
            expect(self_proc->uid.has_value(),
                   "self process UID must be populated on HP-UX");
            expect(self_proc->gid.has_value(),
                   "self process GID must be populated on HP-UX");
            expect(!self_proc->thread_count.has_value(),
                   "thread count must be nullopt on HP-UX");
            if (self_proc->resident_memory_bytes &&
                self_proc->virtual_memory_bytes) {
                expect(*self_proc->resident_memory_bytes > 0,
                       "resident memory must be positive");
                expect(*self_proc->resident_memory_bytes == 4997120ULL,
                       "resident memory must include every resident segment");
                expect(*self_proc->virtual_memory_bytes >=
                           *self_proc->resident_memory_bytes,
                       "virtual memory must be >= resident memory");
            }
        }
    }

    const auto p0 = syscape::process_list::find_process(0U);
    expect(p0.error() == syscape::errc::not_found,
           "find_process for PID 0 must report not_found");

    const auto p_none = syscape::process_list::find_process(99999999U);
    expect(p_none.error() == syscape::errc::not_found,
           "find_process for nonexistent PID must report not_found");

    const auto matches = syscape::process_list::find_processes_by_name(
        "non_existent_12345_proc");
    expect((matches && matches->empty()) ||
               matches.error() == syscape::errc::not_supported,
           "find_processes_by_name for non-existent name must return empty "
           "list or not_supported");

    const auto name_matches =
        syscape::process_list::find_processes_by_name("syscape_proc");
    expect(name_matches.has_value(), "find_processes_by_name must succeed");
    if (name_matches) {
        expect(!name_matches->empty(),
               "find_processes_by_name should find syscape_proc by ucomm");
    }

    const auto cmd_matches = syscape::process_list::find_processes_by_name(
        "/usr/bin/syscape_proc --test-flag");
    expect(cmd_matches.has_value() && cmd_matches->empty(),
           "find_processes_by_name must not match full command line arguments");

#if defined(SYSCAPE_HPUX_PSTAT_MOCK)
    ::setenv("SYSCAPE_TEST_PSTAT_PROC_BACKWARD", "1", 1);
    expect(syscape::process_list::processes().error() ==
               syscape::errc::malformed_data,
           "process enumeration must reject a non-advancing index");
    ::unsetenv("SYSCAPE_TEST_PSTAT_PROC_BACKWARD");

    ::setenv("SYSCAPE_TEST_PSTAT_PROC_ID_OVERFLOW", "1", 1);
    expect(syscape::process_list::processes().error() ==
               syscape::errc::value_too_large,
           "an overflowing process identity must report value_too_large");
    if (my_pid) {
        expect(syscape::process_list::find_process(*my_pid).error() ==
                   syscape::errc::value_too_large,
               "find_process must reject an overflowing returned PID");
    }
    ::unsetenv("SYSCAPE_TEST_PSTAT_PROC_ID_OVERFLOW");

    ::setenv("SYSCAPE_TEST_PSTAT_PROC_ID_NEGATIVE", "1", 1);
    expect(syscape::process_list::processes().error() ==
               syscape::errc::malformed_data,
           "a negative process identity must report malformed_data");
    ::unsetenv("SYSCAPE_TEST_PSTAT_PROC_ID_NEGATIVE");
#endif
}

} // namespace

int main() {
    test_process_list_queries();
    return failures == 0 ? 0 : 1;
}
