#include <system_error>

#include <syscape/architecture.hpp>
#include <syscape/cpu.hpp>
#include <syscape/error.hpp>
#include <syscape/memory.hpp>
#include <syscape/os.hpp>
#include <syscape/process.hpp>
#include <syscape/result.hpp>

const std::error_category* other_error_category() {
    return &syscape::error_category();
}

syscape::architecture other_architecture() {
    return syscape::target_architecture();
}

bool other_os_backend_callable() {
    const syscape::result<std::string> value = syscape::os::kernel_name();
    return value || value.error() == std::errc::operation_not_supported;
}

bool other_cpu_backend_callable() {
    const syscape::result<std::uint32_t> value =
        syscape::cpu::online_logical_processor_count();
    return (value && *value > 0U) ||
           value.error() == std::errc::operation_not_supported;
}

bool other_memory_backend_callable() {
    const syscape::result<std::uint64_t> value =
        syscape::memory::physical_memory_bytes();
    return (value && *value > 0U) ||
           value.error() == std::errc::operation_not_supported;
}

bool other_process_backend_callable() {
    const syscape::result<std::uint32_t> process =
        syscape::process::process_id();
    const syscape::result<std::uint32_t> parent =
        syscape::process::parent_process_id();
    const syscape::result<std::string> executable =
        syscape::process::executable_path();
    const syscape::result<std::vector<std::string>> arguments =
        syscape::process::command_line();
    const syscape::result<std::string> directory =
        syscape::process::working_directory();
    static_cast<void>(parent);
    static_cast<void>(executable);
    static_cast<void>(arguments);
    static_cast<void>(directory);
    return (process && *process > 0U) ||
           process.error() == std::errc::operation_not_supported;
}
