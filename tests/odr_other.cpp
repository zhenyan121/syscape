#include <system_error>

#include <syscape/architecture.hpp>
#include <syscape/cpu.hpp>
#include <syscape/error.hpp>
#include <syscape/memory.hpp>
#include <syscape/os.hpp>
#include <syscape/process.hpp>
#include <syscape/result.hpp>
#include <syscape/user.hpp>

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

bool other_user_backend_callable() {
    const syscape::result<std::uint32_t> real_user =
        syscape::user::real_user_id();
    const syscape::result<std::uint32_t> effective_user =
        syscape::user::effective_user_id();
    const syscape::result<std::uint32_t> real_group =
        syscape::user::real_group_id();
    const syscape::result<std::uint32_t> effective_group =
        syscape::user::effective_group_id();
    const syscape::result<std::string> name = syscape::user::user_name();
    const syscape::result<std::string> home =
        syscape::user::home_directory();
    const syscape::result<std::string> shell = syscape::user::shell();
    static_cast<void>(effective_user);
    static_cast<void>(real_group);
    static_cast<void>(effective_group);
    static_cast<void>(name);
    static_cast<void>(home);
    static_cast<void>(shell);
    return real_user.has_value() ||
           real_user.error() == std::errc::operation_not_supported;
}
