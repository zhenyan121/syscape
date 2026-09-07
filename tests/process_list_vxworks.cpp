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
           "process_count must report not_supported on VxWorks");

    const auto procs = syscape::process_list::processes();
    expect(procs.error() == syscape::errc::not_supported,
           "processes must report not_supported on VxWorks");

    const auto p = syscape::process_list::find_process(1U);
    expect(p.error() == syscape::errc::not_supported,
           "find_process must report not_supported on VxWorks");

    const auto matches = syscape::process_list::find_processes_by_name("init");
    expect(matches.error() == syscape::errc::not_supported,
           "find_processes_by_name must report not_supported on VxWorks");
}

} // namespace

int main() {
    test_process_list_queries();
    return failures == 0 ? 0 : 1;
}
