#include <string>
#include <system_error>

#include <syscape/environment.hpp>
#include <syscape/detail/environment/macos.hpp>

int main() {
    const syscape::result<std::string> path =
        syscape::environment::get("PATH");
    const syscape::result<bool> has_path =
        syscape::environment::has("PATH");
    const syscape::result<std::vector<syscape::environment::environment_variable>> vars =
        syscape::environment::environment_variables();
    const syscape::result<std::string> cwd =
        syscape::environment::current_working_directory();
    const syscape::result<std::string> ls =
        syscape::environment::find_executable("ls");
    const syscape::result<std::string> tmp =
        syscape::environment::temp_directory();
    const syscape::result<std::string> home =
        syscape::environment::home_directory();
    const syscape::result<std::string> cfg =
        syscape::environment::config_directory();
    const syscape::result<std::string> data =
        syscape::environment::data_directory();
    const syscape::result<std::string> cache =
        syscape::environment::cache_directory();
    const syscape::result<bool> is_in =
        syscape::environment::is_interactive_stdin();
    const syscape::result<bool> is_out =
        syscape::environment::is_interactive_stdout();
    const syscape::result<bool> is_err =
        syscape::environment::is_interactive_stderr();

    constexpr char list_sep = syscape::environment::path_list_separator();
    constexpr char dir_sep = syscape::environment::directory_separator();
    static_cast<void>(list_sep);
    static_cast<void>(dir_sep);

    static_cast<void>(path);
    static_cast<void>(has_path);
    static_cast<void>(vars);
    static_cast<void>(cwd);
    static_cast<void>(ls);
    static_cast<void>(tmp);
    static_cast<void>(home);
    static_cast<void>(cfg);
    static_cast<void>(data);
    static_cast<void>(cache);
    static_cast<void>(is_in);
    static_cast<void>(is_out);
    static_cast<void>(is_err);
    return 0;
}
