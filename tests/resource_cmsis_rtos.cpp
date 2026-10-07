#include <iostream>

#include "cmsis_os2.h"

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
           "thread count must report 4 from osThreadGetCount");

    syscape_test_set_cmsis_rtos_thread_count(9U);
    const auto threads2 = syscape::resource::thread_count();
    expect(threads2.has_value() && *threads2 == 9U,
           "thread count must dynamically reflect osThreadGetCount");

    syscape_test_reset_cmsis_rtos_mock();

    const auto load = syscape::resource::load_average();
    expect(!load && load.error() == syscape::errc::not_supported,
           "load average must report not_supported on CMSIS-RTOS");

    const auto entities = syscape::resource::scheduler_entities();
    expect(!entities && entities.error() == syscape::errc::not_supported,
           "scheduler entities must report not_supported on CMSIS-RTOS");

    const auto proc = syscape::resource::process_count();
    expect(!proc && proc.error() == syscape::errc::not_supported,
           "process count must report not_supported on CMSIS-RTOS");

    const auto files = syscape::resource::open_file_count();
    expect(!files && files.error() == syscape::errc::not_supported,
           "open file count must report not_supported on CMSIS-RTOS");

    const auto handles = syscape::resource::open_handle_count();
    expect(!handles && handles.error() == syscape::errc::not_supported,
           "open handle count must report not_supported on CMSIS-RTOS");

    const auto limit = syscape::resource::file_descriptor_limit();
    expect(!limit && limit.error() == syscape::errc::not_supported,
           "file descriptor limit must report not_supported on CMSIS-RTOS");
}

} // namespace

int main() {
    test_resource_queries();
    return failures == 0 ? 0 : 1;
}
