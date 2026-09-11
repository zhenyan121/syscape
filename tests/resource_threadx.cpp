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
    const auto threads = syscape::resource::thread_count();
    expect(threads.has_value() && *threads == 6U,
           "thread count must match mock _tx_thread_created_count on ThreadX");

    const auto procs = syscape::resource::process_count();
    expect(!procs && procs.error() == syscape::errc::not_supported,
           "process count must report not_supported on ThreadX");

    const auto load = syscape::resource::load_average();
    expect(!load && load.error() == syscape::errc::not_supported,
           "load average must report not_supported on ThreadX");

    const auto sched = syscape::resource::scheduler_entities();
    expect(!sched && sched.error() == syscape::errc::not_supported,
           "scheduler entities must report not_supported on ThreadX");

    const auto files = syscape::resource::open_file_count();
    expect(!files && files.error() == syscape::errc::not_supported,
           "open file count must report not_supported on ThreadX");

    const auto handles = syscape::resource::open_handle_count();
    expect(!handles && handles.error() == syscape::errc::not_supported,
           "open handle count must report not_supported on ThreadX");

    const auto fd_lim = syscape::resource::file_descriptor_limit();
    expect(!fd_lim && fd_lim.error() == syscape::errc::not_supported,
           "file descriptor limit must report not_supported on ThreadX");
}

} // namespace

int main() {
    test_resource_queries();
    return failures == 0 ? 0 : 1;
}
