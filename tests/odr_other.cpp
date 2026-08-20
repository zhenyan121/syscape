#include <system_error>

#include <syscape/architecture.hpp>
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
