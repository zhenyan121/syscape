#include <iostream>

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
    expect(count.error() == syscape::errc::not_supported,
           "process count query must report not_supported on Zephyr");

    const auto procs = syscape::process_list::processes();
    expect(procs.error() == syscape::errc::not_supported,
           "processes query must report not_supported on Zephyr");

    const auto p_none = syscape::process_list::find_process(1U);
    expect(p_none.error() == syscape::errc::not_supported,
           "find_process must report not_supported on Zephyr");

    const auto matches = syscape::process_list::find_processes_by_name("init");
    expect(matches.error() == syscape::errc::not_supported,
           "name lookup must report not_supported on Zephyr");

    const auto empty_matches =
        syscape::process_list::find_processes_by_name("");
    expect(empty_matches.error() == syscape::errc::not_supported,
           "empty name lookup must report not_supported on Zephyr");
}

} // namespace

int main() {
    test_process_list_queries();
    return failures == 0 ? 0 : 1;
}
