#include <iostream>

#include "mbed.h"

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
    expect(threads.has_value() && *threads == 4U,
           "thread count must match mock thread count on Mbed OS");

    mbed_mock_set_thread_count(8U);
    const auto threads2 = syscape::resource::thread_count();
    expect(threads2.has_value() && *threads2 == 8U,
           "thread count must dynamically reflect updated count");
    mbed_mock_set_thread_count(4U);

    const auto load = syscape::resource::load_average();
    expect(!load && load.error() == syscape::errc::not_supported,
           "load average must report not_supported on Mbed OS");

    const auto entities = syscape::resource::scheduler_entities();
    expect(!entities && entities.error() == syscape::errc::not_supported,
           "scheduler entities must report not_supported on Mbed OS");

    const auto procs = syscape::resource::process_count();
    expect(!procs && procs.error() == syscape::errc::not_supported,
           "process count must report not_supported on Mbed OS");

    const auto files = syscape::resource::open_file_count();
    expect(!files && files.error() == syscape::errc::not_supported,
           "open file count must report not_supported on Mbed OS");

    const auto handles = syscape::resource::open_handle_count();
    expect(!handles && handles.error() == syscape::errc::not_supported,
           "open handle count must report not_supported on Mbed OS");

    const auto limit = syscape::resource::file_descriptor_limit();
    expect(!limit && limit.error() == syscape::errc::not_supported,
           "file descriptor limit must report not_supported on Mbed OS");
}

} // namespace

int main() {
    test_resource_queries();
    return failures == 0 ? 0 : 1;
}
