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
    expect(matches.error() == syscape::errc::not_supported,
           "name lookup must report not_supported on SerenityOS");

    const auto empty_matches =
        syscape::process_list::find_processes_by_name("");
    expect(empty_matches.error() == syscape::errc::not_supported,
           "empty name lookup must report not_supported on SerenityOS");
}

} // namespace

int main() {
    test_process_list_queries();
    return failures == 0 ? 0 : 1;
}
