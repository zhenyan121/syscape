#include <system_error>

#include <syscape/architecture.hpp>
#include <syscape/cpu.hpp>
#include <syscape/error.hpp>
#include <syscape/memory.hpp>
#include <syscape/os.hpp>
#include <syscape/process.hpp>
#include <syscape/result.hpp>

const std::error_category* other_error_category();
syscape::architecture other_architecture();
bool other_os_backend_callable();
bool other_cpu_backend_callable();
bool other_memory_backend_callable();
bool other_process_backend_callable();

int main() {
    if (other_error_category() != &syscape::error_category()) {
        return 1;
    }
    if (other_architecture() != syscape::target_architecture()) {
        return 2;
    }
    if (!other_os_backend_callable()) {
        return 3;
    }
    if (!other_cpu_backend_callable()) {
        return 4;
    }
    if (!other_memory_backend_callable()) {
        return 5;
    }
    if (!other_process_backend_callable()) {
        return 6;
    }
    const syscape::result<int> value(7);
    return value && *value == 7 ? 0 : 7;
}
