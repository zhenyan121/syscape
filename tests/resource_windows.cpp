#include <cstdint>
#include <iostream>
#include <system_error>

#include <syscape/detail/resource/windows.hpp>
#include <syscape/resource.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_performance_interpreter() {
    namespace backend = syscape::detail::resource_backend;

    const auto normal = backend::interpret_performance_information(
        100000U, 250U, 3200U);
    expect(normal && normal->handle_count == 100000U &&
               normal->process_count == 250U && normal->thread_count == 3200U,
           "A documented performance snapshot must map onto the portable "
           "totals");

    // A zero handle count is accepted as data; a live system always owns
    // processes and threads, so zeros there are malformed platform data.
    const auto zero_handles =
        backend::interpret_performance_information(0U, 250U, 3200U);
    expect(zero_handles && zero_handles->handle_count == 0U,
           "Zero open handles are valid data, not an error sentinel");

    const auto zero_processes =
        backend::interpret_performance_information(10U, 0U, 3200U);
    expect(!zero_processes &&
               zero_processes.error() == syscape::errc::malformed_data,
           "A zero process count cannot describe the running system");

    const auto zero_threads =
        backend::interpret_performance_information(10U, 250U, 0U);
    expect(!zero_threads &&
               zero_threads.error() == syscape::errc::malformed_data,
           "A zero thread count cannot describe the running system");
}

void test_live_queries() {
    const auto handles = syscape::resource::open_handle_count();
    expect(handles.has_value(),
           "Windows must report the documented GetPerformanceInfo handle "
           "total");
}

void test_unsupported_queries() {
    const auto loads = syscape::resource::load_average();
    expect(!loads && loads.error() ==
                         std::errc::operation_not_supported,
           "Windows must not present utilization counters as load averages");

    const auto entities = syscape::resource::scheduler_entities();
    expect(!entities && entities.error() ==
                            std::errc::operation_not_supported,
           "Windows must report scheduling entities as unsupported");

    const auto open_files = syscape::resource::open_file_count();
    expect(!open_files &&
               open_files.error() == std::errc::operation_not_supported,
           "Windows must not present its all-handles total as an "
           "open-file count");

    const auto limit = syscape::resource::file_descriptor_limit();
    expect(!limit &&
               limit.error() == std::errc::operation_not_supported,
           "Windows must not present an implementation bound as a "
           "documented handle limit");
}

} // namespace

int main() {
    test_performance_interpreter();
    test_live_queries();
    test_unsupported_queries();
    return failures == 0 ? 0 : 1;
}
