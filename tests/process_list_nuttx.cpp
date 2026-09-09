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
    syscape::process_list::process_entry parsed;
    const auto status =
        syscape::detail::process_list_backend::nuttx_parse_status(
            "Name:       worker\nState:      Waiting,Semaphore\n", parsed);
    expect(status && parsed.name && *parsed.name == "worker" &&
               parsed.state == syscape::process_list::process_state::sleeping,
           "NuttX procfs status fields must be parsed");

    const auto zero = syscape::process_list::find_process(0U);
    expect(!zero && zero.error() == syscape::errc::not_found,
           "PID zero must not be directly queryable");

    const auto count = syscape::process_list::process_count();
    expect(count.has_value() || count.error() == syscape::errc::not_supported ||
               count.error() == syscape::errc::permission_denied,
           "process count query must succeed or report error");

    const auto procs = syscape::process_list::processes();
    expect(procs.has_value() || procs.error() == syscape::errc::not_supported ||
               procs.error() == syscape::errc::permission_denied,
           "processes query must succeed or report error");
    if (procs) {
        for (std::size_t i = 1; i < procs->size(); ++i) {
            expect(
                (*procs)[i - 1].pid < (*procs)[i].pid,
                "processes must be sorted in natural ascending order by PID");
        }
        for (const auto& proc : *procs) {
            expect(!proc.uid.has_value(), "uid must remain nullopt on NuttX");
            expect(!proc.gid.has_value(), "gid must remain nullopt on NuttX");
            expect(!proc.user_name.has_value(),
                   "user_name must remain nullopt on NuttX");
            expect(!proc.ppid.has_value(), "ppid must remain nullopt on NuttX");
        }
    }

    const auto my_pid = syscape::process::process_id();
    if (my_pid && procs && !procs->empty()) {
        const auto self_proc = syscape::process_list::find_process(*my_pid);
        expect(self_proc.has_value() ||
                   self_proc.error() == syscape::errc::not_found,
               "find_process for current process must succeed or report "
               "not_found");
    }

    const auto p_none = syscape::process_list::find_process(99999999U);
    expect(p_none.error() == syscape::errc::not_found ||
               p_none.error() == syscape::errc::not_supported,
           "find_process for nonexistent PID must report not_found");

    const auto matches = syscape::process_list::find_processes_by_name(
        "__nonexistent_proc_xyz_123__");
    expect(matches && matches->empty(),
           "unknown process name must return an empty collection");

    const auto empty_matches =
        syscape::process_list::find_processes_by_name("");
    expect(!empty_matches &&
               empty_matches.error() == syscape::errc::invalid_argument,
           "empty name lookup must report invalid_argument");
}

} // namespace

int main() {
    test_process_list_queries();
    return failures == 0 ? 0 : 1;
}
