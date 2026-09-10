#include <iostream>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

#include <syscape/environment.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_environment_queries() {
    const auto cwd = syscape::environment::current_working_directory();
    expect(cwd.has_value() || cwd.error() == syscape::errc::not_supported,
           "current_working_directory must succeed or report not_supported");

    const auto vars = syscape::environment::environment_variables();
    expect(vars.has_value(), "environment_variables must succeed");

    const auto missing =
        syscape::environment::get("SYSCAPE_NONEXISTENT_VAR_Emscripten");
    expect(!missing && missing.error() == syscape::errc::not_found,
           "missing environment variable must report not_found");

    const auto tmp = syscape::environment::temp_directory();
    expect(tmp.has_value() || tmp.error() == syscape::errc::not_supported ||
               tmp.error() == syscape::errc::permission_denied,
           "temp_directory must succeed or report not_supported / "
           "permission_denied");
    if (tmp.has_value()) {
        expect(!tmp->empty() && tmp->front() == '/',
               "temp_directory when reported must be an absolute path");
    }

    // Test TMPDIR configuration behavior
    const char* old_tmp = ::getenv("TMPDIR");
    const bool had_tmp = old_tmp != nullptr;
    const std::string saved_tmp = had_tmp ? old_tmp : "";

    ::setenv("TMPDIR", "/nonexistent_syscape_dir_12345", 1);
    const auto bad_tmp = syscape::environment::temp_directory();
    expect(!bad_tmp && bad_tmp.error() == syscape::errc::not_found,
           "temp_directory with non-existent TMPDIR must report not_found and "
           "not fallback");

    const char bad_utf8[] = {'/', '\xff', '\xfe', '\0'};
    ::setenv("TMPDIR", bad_utf8, 1);
    const auto enc_tmp = syscape::environment::temp_directory();
    expect(!enc_tmp && enc_tmp.error() == syscape::errc::invalid_encoding,
           "temp_directory with invalid UTF-8 TMPDIR must report "
           "invalid_encoding");

    if (had_tmp) {
        ::setenv("TMPDIR", saved_tmp.c_str(), 1);
    } else {
        ::unsetenv("TMPDIR");
    }

    // Test HOME pointing to a non-directory file and error propagation
    const char* old_home = ::getenv("HOME");
    const bool had_home = old_home != nullptr;
    const std::string saved_home = had_home ? old_home : "";

    const char* old_xdg_cfg = ::getenv("XDG_CONFIG_HOME");
    const bool had_xdg_cfg = old_xdg_cfg != nullptr;
    const std::string saved_xdg_cfg = had_xdg_cfg ? old_xdg_cfg : "";
    ::unsetenv("XDG_CONFIG_HOME");

    const char* old_xdg_data = ::getenv("XDG_DATA_HOME");
    const bool had_xdg_data = old_xdg_data != nullptr;
    const std::string saved_xdg_data = had_xdg_data ? old_xdg_data : "";
    ::unsetenv("XDG_DATA_HOME");

    const char* old_xdg_cache = ::getenv("XDG_CACHE_HOME");
    const bool had_xdg_cache = old_xdg_cache != nullptr;
    const std::string saved_xdg_cache = had_xdg_cache ? old_xdg_cache : "";
    ::unsetenv("XDG_CACHE_HOME");

    const std::string test_file =
        "/tmp/syscape_emscripten_not_a_dir_" + std::to_string(::getpid());
    int test_fd = ::open(test_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (test_fd != -1) {
        ::close(test_fd);
        ::setenv("HOME", test_file.c_str(), 1);
    } else {
        ::setenv("HOME", "/dev/null", 1);
    }
    const auto file_home = syscape::environment::home_directory();
    expect(
        !file_home && file_home.error() == syscape::errc::not_found,
        "home_directory pointing to non-directory file must report not_found");

    const auto cfg_err = syscape::environment::config_directory();
    expect(!cfg_err && cfg_err.error() == syscape::errc::not_found,
           "config_directory must preserve home_directory not_found error");

    const auto data_err = syscape::environment::data_directory();
    expect(!data_err && data_err.error() == syscape::errc::not_found,
           "data_directory must preserve home_directory not_found error");

    const auto cache_err = syscape::environment::cache_directory();
    expect(!cache_err && cache_err.error() == syscape::errc::not_found,
           "cache_directory must preserve home_directory not_found error");

    if (test_fd != -1) {
        ::unlink(test_file.c_str());
    }

    if (had_home) {
        ::setenv("HOME", saved_home.c_str(), 1);
    } else {
        ::unsetenv("HOME");
    }
    if (had_xdg_cfg) {
        ::setenv("XDG_CONFIG_HOME", saved_xdg_cfg.c_str(), 1);
    }
    if (had_xdg_data) {
        ::setenv("XDG_DATA_HOME", saved_xdg_data.c_str(), 1);
    }
    if (had_xdg_cache) {
        ::setenv("XDG_CACHE_HOME", saved_xdg_cache.c_str(), 1);
    }

    // Public input validation for find_executable
    const auto empty_name = syscape::environment::find_executable("");
    expect(!empty_name && empty_name.error() == syscape::errc::invalid_argument,
           "find_executable with empty name must return invalid_argument");

    const auto null_name =
        syscape::environment::find_executable(std::string_view("a\0b", 3));
    expect(!null_name && null_name.error() == syscape::errc::invalid_argument,
           "find_executable with embedded null must return invalid_argument");

    const char invalid_utf8[] = {'\xff', '\xfe', '\0'};
    const auto invalid_utf8_name =
        syscape::environment::find_executable(invalid_utf8);
    expect(!invalid_utf8_name &&
               invalid_utf8_name.error() == syscape::errc::invalid_encoding,
           "find_executable with invalid UTF-8 must return invalid_encoding");

    // Valid executable names must report not_supported on Emscripten
    const auto direct = syscape::environment::find_executable("/bin/sh");
    expect(!direct && direct.error() == syscape::errc::not_supported,
           "find_executable with explicit path must report not_supported on "
           "Emscripten");

    const auto path_based = syscape::environment::find_executable("sh");
    expect(!path_based && path_based.error() == syscape::errc::not_supported,
           "find_executable with command name must report not_supported on "
           "Emscripten");

    const auto in_tty = syscape::environment::is_interactive_stdin();
    expect(in_tty.has_value(), "is_interactive_stdin must succeed");
}

} // namespace

int main() {
    test_environment_queries();
    return failures == 0 ? 0 : 1;
}
