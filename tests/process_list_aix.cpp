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
    expect(procs.has_value() || procs.error() == syscape::errc::not_supported,
           "processes query must succeed or report not_supported");
    if (procs) {
        for (const auto& p : *procs) {
            expect(p.pid > 0, "PID must be positive");
        }
        for (std::size_t i = 1; i < procs->size(); ++i) {
            expect(
                (*procs)[i - 1].pid < (*procs)[i].pid,
                "processes must be sorted in natural ascending order by PID");
        }
    }

    const auto my_pid = syscape::process::process_id();
    if (my_pid) {
        const auto self_proc = syscape::process_list::find_process(*my_pid);
        expect(self_proc.has_value() ||
                   self_proc.error() == syscape::errc::not_supported,
               "find_process for current process must succeed or report "
               "not_supported");
        if (self_proc) {
            expect(self_proc->pid == *my_pid, "PID must match current process");
        }
    }

    const auto p0 = syscape::process_list::find_process(0U);
    expect(p0.error() == syscape::errc::not_found ||
               p0.error() == syscape::errc::not_supported,
           "find_process for PID 0 must report not_found or not_supported");

    const auto matches = syscape::process_list::find_processes_by_name(
        "non_existent_12345_proc");
    expect((matches && matches->empty()) ||
               matches.error() == syscape::errc::not_supported,
           "find_processes_by_name for non-existent name must return empty "
           "list or not_supported");
}

} // namespace

int main() {
    test_process_list_queries();
    return failures == 0 ? 0 : 1;
}
