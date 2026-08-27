#include <string>
#include <system_error>

#include <syscape/environment.hpp>

template <typename T>
bool unsupported(const syscape::result<T>& value) {
    return !value && value.error() == std::errc::operation_not_supported;
}

int main() {
    constexpr char list_sep = syscape::environment::path_list_separator();
    constexpr char dir_sep = syscape::environment::directory_separator();
    static_cast<void>(list_sep);
    static_cast<void>(dir_sep);

    return unsupported(syscape::environment::get("PATH")) &&
                   unsupported(syscape::environment::has("PATH")) &&
                   unsupported(syscape::environment::environment_variables()) &&
                   unsupported(syscape::environment::current_working_directory()) &&
                   unsupported(syscape::environment::find_executable("ls")) &&
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
