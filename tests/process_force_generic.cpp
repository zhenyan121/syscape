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
    const auto invalid_limit = syscape::process::resource_limit(
        static_cast<syscape::process::resource_kind>(999));
    return unsupported(syscape::process::process_id()) &&
                   unsupported(syscape::process::parent_process_id()) &&
                   unsupported(syscape::process::executable_path()) &&
                   unsupported(syscape::process::command_line()) &&
                   unsupported(syscape::process::working_directory()) &&
                   unsupported(syscape::process::cpu_time()) &&
                   unsupported(syscape::process::start_time()) &&
                   unsupported(syscape::process::memory_usage()) &&
                   unsupported(syscape::process::thread_count()) &&
                   unsupported(syscape::process::priority()) &&
                   unsupported(syscape::process::cpu_affinity()) &&
                   unsupported(
                       syscape::process::resource_limit(
                           syscape::process::resource_kind::core_file_size)) &&
                   !invalid_limit &&
                   invalid_limit.error() == syscape::errc::invalid_argument
               ? 0
               : 1;
}
