#include <iostream>
#include <cstdlib>

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
    expect(load.has_value(), "load average query must succeed with pstat");
    if (load) {
        expect(load->one_minute > 0.0,
               "1-minute load average must be positive");
    }

    const auto procs = syscape::resource::process_count();
    expect(procs.has_value(), "process count query must succeed with pstat");
    if (procs) {
        expect(*procs == 42, "process count must be 42 from psd_activeprocs");
    }

    const auto fd_lim = syscape::resource::file_descriptor_limit();
    expect(fd_lim && *fd_lim > 0, "file descriptor limit must be positive");

    const auto entities = syscape::resource::scheduler_entities();
    expect(entities.error() == syscape::errc::not_supported,
           "scheduler entities must report not_supported on HP-UX");

#if defined(SYSCAPE_HPUX_PSTAT_MOCK)
    ::setenv("SYSCAPE_TEST_PSTAT_DYNAMIC_ZERO", "1", 1);
    expect(syscape::resource::load_average().error() ==
               syscape::errc::temporarily_unavailable,
           "an empty dynamic snapshot must not produce a zero load average");
    expect(syscape::resource::process_count().error() ==
               syscape::errc::temporarily_unavailable,
           "an empty dynamic snapshot must not produce a zero process count");
    ::unsetenv("SYSCAPE_TEST_PSTAT_DYNAMIC_ZERO");
#endif
}

} // namespace

int main() {
    test_resource_queries();
    return failures == 0 ? 0 : 1;
}
