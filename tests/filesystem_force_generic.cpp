#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

#include <syscape/filesystem.hpp>

template <typename T>
bool unsupported(const syscape::result<T>& value) {
    return !value && value.error() == std::errc::operation_not_supported;
}

int main() {
    const syscape::result<std::vector<syscape::filesystem::mount_entry>>
        mounted = syscape::filesystem::mounts();
    const syscape::result<syscape::filesystem::space_info> capacity =
        syscape::filesystem::space("/any/path");
    const syscape::result<syscape::filesystem::space_info> empty_path =
        syscape::filesystem::space("");
    const syscape::result<syscape::filesystem::space_info> bad_encoding =
        syscape::filesystem::space(std::string("/\xff", 2U));
    const syscape::result<syscape::filesystem::space_info> embedded_null =
        syscape::filesystem::space(std::string("/tmp\0ignored", 12U));

    const bool input_validated =
        !empty_path &&
        empty_path.error() ==
            syscape::make_error_code(syscape::errc::invalid_argument) &&
        !bad_encoding &&
        bad_encoding.error() ==
            syscape::make_error_code(syscape::errc::invalid_encoding) &&
        !embedded_null &&
        embedded_null.error() ==
            syscape::make_error_code(syscape::errc::invalid_argument);

    // Input validation happens at the public boundary before backend
    // selection, so even the generic fallback must observe it.
    return unsupported(mounted) && unsupported(capacity) && input_validated
               ? 0
               : 1;
}
