#include <cassert>
#include <iostream>
#include <limits>
#include <syscape/process_list.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_macos_process_list_backend() {
    using syscape::detail::process_list_backend::macos_process_state;
    using syscape::process_list::process_state;

    expect(macos_process_state(SRUN) == process_state::running,
           "SRUN must map to running");
    expect(macos_process_state(SSLEEP) == process_state::sleeping,
           "SSLEEP must map to sleeping");
    expect(macos_process_state(SSTOP) == process_state::stopped,
           "SSTOP must map to stopped");
    expect(macos_process_state(SZOMB) == process_state::zombie,
           "SZOMB must map to zombie");
    expect(macos_process_state(SIDL) == process_state::unknown,
           "SIDL must map to unknown");

    ::kinfo_proc credentials{};
    credentials.kp_eproc.e_pcred.p_ruid = 123U;
    credentials.kp_eproc.e_ucred.cr_uid = 456U;
    expect(syscape::detail::process_list_backend::macos_real_uid(credentials) ==
               123U,
           "Process UID must use the real credential");

    const auto list = syscape::process_list::processes();
    if (list) {
        for (const auto& p : *list) {
            expect(p.pid > 0U, "Process PID must be positive");
            expect(p.name.has_value() && !p.name->empty(),
                   "Process name must not be empty");
        }
    } else {
        expect(static_cast<bool>(list.error()),
               "Failure must carry a nonzero error code");
    }

    const auto count = syscape::process_list::process_count();
    expect(count || static_cast<bool>(count.error()),
           "process_count failure must carry an error code");

    const auto find = syscape::process_list::find_process(
        (std::numeric_limits<std::uint32_t>::max)());
    expect(!find, "Nonexistent PID lookup must fail");
    expect(find.error() == syscape::errc::not_found,
           "Invalid PID must return not_found error");

    const auto by_name =
        syscape::process_list::find_processes_by_name("__nonexistent_proc__");
    if (by_name) {
        expect(by_name->empty(), "Nonexistent process name must return empty list");
    } else {
        expect(static_cast<bool>(by_name.error()), "Failure must carry error code");
    }
}

} // namespace

int main() {
    test_macos_process_list_backend();
    return failures == 0 ? 0 : 1;
}
