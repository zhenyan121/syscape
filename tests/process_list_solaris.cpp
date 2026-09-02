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
    expect(count && *count > 0, "process count must be positive");

    const auto procs = syscape::process_list::processes();
    expect(procs && !procs->empty(),
           "processes query must return nonempty list");
    if (procs) {
        for (const auto& p : *procs) {
            expect(p.pid > 0, "PID must be positive");
            expect(!p.command_line.has_value(),
                   "command_line must be nullopt on Solaris procfs psinfo");
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
        expect(self_proc && self_proc->pid == *my_pid,
               "find_process for current process must succeed");
    }

    expect(syscape::process_list::find_process(0U).error() ==
               syscape::errc::not_found,
           "find_process for PID 0 must report not_found");

    const auto matches = syscape::process_list::find_processes_by_name(
        "non_existent_12345_proc");
    expect(
        matches && matches->empty(),
        "find_processes_by_name for non-existent name must return empty list");
}

} // namespace

int main() {
    test_process_list_queries();
    return failures == 0 ? 0 : 1;
}
