#include <system_error>

#include <syscape/architecture.hpp>
#include <syscape/cpu.hpp>
#include <syscape/error.hpp>
#include <syscape/os.hpp>
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
