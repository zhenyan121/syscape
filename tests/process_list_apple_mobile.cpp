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
    expect(count.has_value() ||
               count.error() == syscape::errc::permission_denied ||
               count.error() == syscape::errc::not_supported,
           "process count query must succeed or report expected error");

    const auto procs = syscape::process_list::processes();
    expect(procs.has_value() ||
               procs.error() == syscape::errc::permission_denied ||
               procs.error() == syscape::errc::not_supported,
           "processes query must succeed or report expected error");

    const auto pid0 = syscape::process_list::find_process(0);
    expect(!pid0 && pid0.error() == syscape::errc::not_found,
           "find_process(0) must return not_found");

    const auto single = syscape::process_list::find_process(1);
    expect(single.has_value() ||
               single.error() == syscape::errc::permission_denied ||
               single.error() == syscape::errc::not_supported ||
               single.error() == syscape::errc::not_found,
           "find_process query must succeed or report expected error");

    const auto by_name =
        syscape::process_list::find_processes_by_name("launchd");
    expect(
        by_name.has_value() ||
            by_name.error() == syscape::errc::permission_denied ||
            by_name.error() == syscape::errc::not_supported,
        "find_processes_by_name query must succeed or report expected error");
}

} // namespace

int main() {
    test_process_list_queries();
    return failures == 0 ? 0 : 1;
}
