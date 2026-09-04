#include <iostream>

#include <syscape/resource.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_resource_queries() {
    const auto loads = syscape::resource::load_average();
    expect(loads.has_value() || loads.error() == syscape::errc::not_supported,
           "load average query must succeed or report not_supported");

    const auto limit = syscape::resource::file_descriptor_limit();
    expect(limit.has_value() || limit.error() == syscape::errc::not_supported,
           "file descriptor limit query must succeed or report not_supported");

    const auto open_files = syscape::resource::open_file_count();
    expect(open_files.has_value() ||
               open_files.error() == syscape::errc::not_supported,
           "open file count query must succeed or report not_supported");

    const auto proc_count = syscape::resource::process_count();
    expect(proc_count.has_value() ||
               proc_count.error() == syscape::errc::not_supported,
           "process count query must succeed or report not_supported");
}

} // namespace

int main() {
    test_resource_queries();
    return failures == 0 ? 0 : 1;
}
