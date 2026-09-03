#include <chrono>
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
    expect((count && *count > 0) ||
               count.error() == syscape::errc::not_supported,
           "process count must be positive or not_supported");
    const auto procs = syscape::process_list::processes();
    expect((procs && !procs->empty()) ||
               procs.error() == syscape::errc::not_supported,
           "processes query must return nonempty list or not_supported");
    if (procs) {
        for (const auto& p : *procs) {
            expect(p.pid > 0, "PID must be positive");
            expect(!p.command_line.has_value(),
                   "command_line must be nullopt on Haiku");
            expect(p.state == syscape::process_list::process_state::unknown,
                   "process state must be unknown on Haiku");
            if (p.executable_path) {
                expect(!p.executable_path->empty() &&
                           p.executable_path->front() == '/',
                       "executable_path when present must be an absolute path");
            }
            if (p.start_time) {
                const auto epoch_s =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        p.start_time->time_since_epoch())
                        .count();
                expect(epoch_s > 1577836800LL,
                       "start_time must be realistic wall-clock timestamp");
            }
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
        expect((self_proc && self_proc->pid == *my_pid) ||
                   self_proc.error() == syscape::errc::not_supported,
               "find_process for current process must succeed or report "
               "not_supported");
    }
    expect(syscape::process_list::find_process(0U).error() ==
               syscape::errc::not_found,
           "find_process for PID 0 must report not_found");
}

} // namespace

int main() {
    test_process_list_queries();
    return failures == 0 ? 0 : 1;
}
