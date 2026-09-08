#include <cstdint>
#include <iostream>

#include <unistd.h>

// Simulate Zephyr's CONFIG_POSIX_SYSCONF_IMPL_MACRO where sysconf is a macro:
// #define sysconf(x) (long)CONCAT(...)
#define MOCK_PAGE_SIZE 4096L
#define MOCK_OPEN_MAX 256L
#define MOCK_NPROCESSORS 4L

#undef sysconf
#define sysconf(x)                                                             \
    ((x) == _SC_PAGESIZE                                                       \
         ? MOCK_PAGE_SIZE                                                      \
         : ((x) == _SC_OPEN_MAX                                                \
                ? MOCK_OPEN_MAX                                                \
                : ((x) == _SC_NPROCESSORS_ONLN ? MOCK_NPROCESSORS : -1L)))

#include <syscape/cpu.hpp>
#include <syscape/memory.hpp>
#include <syscape/resource.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_sysconf_macro() {
    const auto page_size = syscape::memory::page_size_bytes();
    expect(page_size.has_value() &&
               *page_size == static_cast<std::uint64_t>(MOCK_PAGE_SIZE),
           "page_size_bytes must evaluate correctly via sysconf macro");

    const auto fd_lim = syscape::resource::file_descriptor_limit();
    expect(fd_lim.has_value() &&
               *fd_lim == static_cast<std::uint64_t>(MOCK_OPEN_MAX),
           "file_descriptor_limit must evaluate correctly via sysconf macro");

    const auto cpu_count = syscape::cpu::online_logical_processor_count();
    expect(cpu_count.has_value() &&
               *cpu_count == static_cast<std::uint32_t>(MOCK_NPROCESSORS),
           "online_logical_processor_count must evaluate correctly via "
           "sysconf macro");
}

} // namespace

int main() {
    test_sysconf_macro();
    return failures == 0 ? 0 : 1;
}
