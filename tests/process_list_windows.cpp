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

void test_windows_process_list_backend() {
    syscape::process_list::process_entry synthetic;
    synthetic.name = "CaseInsensitiveName.exe";
    expect(syscape::detail::process_list_common::matches_process_name(
               synthetic, "caseinsensitivename.EXE", true),
           "Windows name matching must ignore ASCII case");

    const auto list = syscape::process_list::processes();
    if (list) {
        for (const auto& p : *list) {
            expect(p.pid > 0U, "Enumerated process PID must be positive");
            expect(p.name.has_value() && !p.name->empty(),
                   "Process name must not be empty");
            expect(p.state == syscape::process_list::process_state::unknown,
                   "Toolhelp snapshot must not invent process state");
            expect(!p.virtual_memory_bytes.has_value(),
                   "Unsupported virtual size must remain unavailable");
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
    if (list) {
        expect(find.error() == syscape::errc::not_found,
               "Nonexistent PID must return not_found error");
    } else {
        expect(find.error() == list.error(),
               "Lookup must preserve enumeration failures");
    }

    const auto zero = syscape::process_list::find_process(0U);
    expect(!zero && zero.error() == syscape::errc::not_found,
           "PID zero must consistently return not_found");

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
    test_windows_process_list_backend();
    return failures == 0 ? 0 : 1;
}
