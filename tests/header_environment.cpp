#include <string>

#include <syscape/environment.hpp>
#include <syscape/environment.hpp>

int main() {
    const syscape::result<std::string> path =
        syscape::environment::get("PATH");
    const syscape::result<bool> has_path =
        syscape::environment::has("PATH");
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

    static_cast<void>(path);
    static_cast<void>(has_path);
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
