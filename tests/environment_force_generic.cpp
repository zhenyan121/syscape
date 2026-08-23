#include <string>
#include <system_error>

#include <syscape/environment.hpp>

template <typename T>
bool unsupported(const syscape::result<T>& value) {
    return !value && value.error() == std::errc::operation_not_supported;
}

int main() {
    return unsupported(syscape::environment::get("PATH")) &&
                   unsupported(syscape::environment::has("PATH")) &&
                   unsupported(syscape::environment::temp_directory()) &&
                   unsupported(syscape::environment::home_directory()) &&
                   unsupported(syscape::environment::config_directory()) &&
                   unsupported(syscape::environment::data_directory()) &&
                   unsupported(syscape::environment::cache_directory()) &&
                   unsupported(syscape::environment::is_interactive_stdin()) &&
                   unsupported(syscape::environment::is_interactive_stdout()) &&
                   unsupported(syscape::environment::is_interactive_stderr())
               ? 0
               : 1;
}
