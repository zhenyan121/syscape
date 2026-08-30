#include <string>
#include <system_error>

#include <syscape/environment.hpp>
#include <syscape/detail/environment/windows.hpp>

int main() {
    const auto empty_value =
        syscape::detail::environment_backend::wide_to_utf8(L"");
    if (!empty_value || !empty_value->empty()) {
        return 2;
    }

    const auto ascii =
        syscape::detail::environment_backend::wide_to_utf8(L"profile");
    if (!ascii || *ascii != "profile") { return 3; }

    const auto supplementary =
        syscape::detail::environment_backend::wide_to_utf8(
            std::wstring(1U, static_cast<wchar_t>(0x00E9U)));
    if (!supplementary || *supplementary != "\xC3\xA9") { return 4; }

    std::wstring lone_surrogate(1U, static_cast<wchar_t>(0xD800U));
    if (syscape::detail::environment_backend::wide_to_utf8(lone_surrogate)
            .error() != syscape::errc::invalid_encoding) {
        return 5;
    }

    const auto denied = syscape::detail::environment_backend::map_hresult(
        ::HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED));
    if (!(denied == std::errc::permission_denied)) { return 6; }

    const auto not_found = syscape::detail::environment_backend::map_hresult(
        ::HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    if (not_found.value() != ERROR_FILE_NOT_FOUND ||
        &not_found.category() != &std::system_category()) {
        return 7;
    }

    const auto generic_failure =
        syscape::detail::environment_backend::map_hresult(E_FAIL);
    if (generic_failure.value() != static_cast<int>(E_FAIL) ||
        generic_failure.message().empty() ||
        &generic_failure.category() !=
            &syscape::detail::environment_backend::hresult_category()) {
        return 8;
    }

    const syscape::result<std::string> path =
        syscape::environment::get("PATH");
    const syscape::result<bool> has_path =
        syscape::environment::has("PATH");
    const syscape::result<std::vector<syscape::environment::environment_variable>> vars =
        syscape::environment::environment_variables();
    const syscape::result<std::string> cwd =
        syscape::environment::current_working_directory();
    const syscape::result<std::string> cmd_exe =
        syscape::environment::find_executable("cmd");
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
    static_cast<void>(cmd_exe);
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
