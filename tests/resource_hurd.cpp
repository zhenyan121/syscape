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
    expect(load.has_value() || load.error() == syscape::errc::not_supported,
           "load average query must succeed or report not_supported");

    const auto fd_lim = syscape::resource::file_descriptor_limit();
    expect(fd_lim && *fd_lim > 0, "file descriptor limit must be positive");

    const auto procs = syscape::resource::process_count();
    expect((procs.has_value() && *procs > 0) ||
               procs.error() == syscape::errc::not_supported ||
               procs.error() == syscape::errc::permission_denied,
           "process count query must succeed with positive value, or report "
           "permission_denied or not_supported");

    const auto entities = syscape::resource::scheduler_entities();
    expect(entities.error() == syscape::errc::not_supported,
           "scheduler entities must report not_supported on GNU/Hurd");

    const auto threads = syscape::resource::thread_count();
    expect(threads.error() == syscape::errc::not_supported,
           "thread count must report not_supported on GNU/Hurd");

    const auto open_files = syscape::resource::open_file_count();
    expect(open_files.error() == syscape::errc::not_supported,
           "open file count must report not_supported on GNU/Hurd");

    const auto open_handles = syscape::resource::open_handle_count();
    expect(open_handles.error() == syscape::errc::not_supported,
           "open handle count must report not_supported on GNU/Hurd");
}

} // namespace

int main() {
    test_resource_queries();
    return failures == 0 ? 0 : 1;
}
