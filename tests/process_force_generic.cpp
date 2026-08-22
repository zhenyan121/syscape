#include <chrono>
#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

#include <syscape/process.hpp>

template <typename T>
bool unsupported(const syscape::result<T>& value) {
    return !value && value.error() == std::errc::operation_not_supported;
}

int main() {
    return unsupported(syscape::process::process_id()) &&
                   unsupported(syscape::process::parent_process_id()) &&
                   unsupported(syscape::process::executable_path()) &&
                   unsupported(syscape::process::command_line()) &&
                   unsupported(syscape::process::working_directory()) &&
                   unsupported(syscape::process::cpu_time()) &&
                   unsupported(syscape::process::start_time()) &&
                   unsupported(syscape::process::memory_usage()) &&
                   unsupported(syscape::process::thread_count())
               ? 0
               : 1;
}
