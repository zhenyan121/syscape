#include <string>
#include <system_error>

#include <syscape/os.hpp>

template <typename T>
bool unsupported(const syscape::result<T>& value) {
    return !value && value.error() == std::errc::operation_not_supported;
}

int main() {
    return unsupported(syscape::os::product_name()) &&
                   unsupported(syscape::os::product_version()) &&
                   unsupported(syscape::os::build_identifier()) &&
                   unsupported(syscape::os::kernel_name()) &&
                   unsupported(syscape::os::kernel_version()) &&
                   unsupported(syscape::os::host_name()) &&
                   unsupported(syscape::os::boot_identifier()) &&
                   unsupported(syscape::os::uptime()) &&
                   unsupported(syscape::os::boot_time())
               ? 0
               : 1;
}
