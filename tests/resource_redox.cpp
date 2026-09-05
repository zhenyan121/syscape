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
    const auto load = syscape::resource::load_average();
    expect(load.error() == syscape::errc::not_supported,
           "load average must report not_supported on Redox OS");

    const auto fd_lim = syscape::resource::file_descriptor_limit();
    expect(fd_lim.error() == syscape::errc::not_supported,
           "file descriptor limit must report not_supported on Redox OS");

    const auto procs = syscape::resource::process_count();
    expect(procs.error() == syscape::errc::not_supported,
           "process count must report not_supported on Redox OS");

    const auto entities = syscape::resource::scheduler_entities();
    expect(entities.error() == syscape::errc::not_supported,
           "scheduler entities must report not_supported on Redox OS");

    const auto threads = syscape::resource::thread_count();
    expect(threads.error() == syscape::errc::not_supported,
           "thread count must report not_supported on Redox OS");

    const auto open_files = syscape::resource::open_file_count();
    expect(open_files.error() == syscape::errc::not_supported,
           "open file count must report not_supported on Redox OS");

    const auto open_handles = syscape::resource::open_handle_count();
    expect(open_handles.error() == syscape::errc::not_supported,
           "open handle count must report not_supported on Redox OS");
}

} // namespace

int main() {
    test_resource_queries();
    return failures == 0 ? 0 : 1;
}
