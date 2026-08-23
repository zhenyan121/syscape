#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <system_error>

#include <syscape/detail/resource/macos.hpp>
#include <syscape/resource.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_nonnegative_scalars() {
    namespace backend = syscape::detail::resource_backend;

    const auto positive = backend::checked_nonnegative(5);
    expect(positive && *positive == 5,
           "A nonnegative scalar must pass through unchanged");

    const auto zero = backend::checked_nonnegative(0);
    expect(zero && *zero == 0,
           "Zero is valid count data, not an error sentinel");

    const auto negative = backend::checked_nonnegative(-1);
    expect(!negative && negative.error() == syscape::errc::malformed_data,
           "A negative reading from a count-valued sysctl must be malformed");
}

void test_unsupported_queries() {
    const auto entities = syscape::resource::scheduler_entities();
    expect(!entities &&
               entities.error() == std::errc::operation_not_supported,
           "macOS must report scheduling entities as unsupported because "
           "Darwin exposes no documented source");
}

} // namespace

int main() {
    test_nonnegative_scalars();
    test_unsupported_queries();

    using syscape::resource::file_descriptor_limit;
    using syscape::resource::load_average;
    using syscape::resource::open_file_count;
    using syscape::resource::process_count;
    using syscape::resource::thread_count;

    const auto loads = load_average();
    expect(loads.has_value(),
           "macOS must read the documented getloadavg interface");
    if (loads) {
        double reference[3] = {0.0, 0.0, 0.0};
        if (::getloadavg(reference, 3) == 3) {
            expect(std::fabs(loads->one_minute - reference[0]) < 0.5 &&
                       std::fabs(loads->five_minute - reference[1]) < 0.5 &&
                       std::fabs(loads->fifteen_minute - reference[2]) < 0.5,
                   "Returned samples must match an independent getloadavg "
                   "read");
        }
    }

    const auto processes = process_count();
    expect(processes && *processes > 0U,
           "macOS must enumerate at least the calling process in the "
           "KERN_PROC_ALL table");

    // The kern.num_threads source must succeed instead of having its
    // failure excused.
    const auto threads = thread_count();
    expect(threads.has_value(),
           "macOS must report the system-wide kern.num_threads total");
    if (threads) {
        expect(*threads > 0U,
               "macOS must report a positive system-wide thread total");
    }

    const auto open_files = open_file_count();
    expect(open_files.has_value(),
           "macOS must read the kern.num_files usage value");

    const auto handles = syscape::resource::open_handle_count();
    expect(!handles &&
               handles.error() == std::errc::operation_not_supported,
           "macOS must not present its file-oriented count as an "
           "all-handles total");

    const auto limit = file_descriptor_limit();
    expect(limit && *limit > 0U,
           "macOS must report a positive kern.maxfiles limit");

    return failures == 0 ? 0 : 1;
}
