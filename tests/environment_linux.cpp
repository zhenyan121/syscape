#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

#include <syscape/environment.hpp>
#include <syscape/detail/environment/common.hpp>
#include <syscape/detail/environment/linux.hpp>
#include <syscape/detail/environment/posix.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_environment_variables() {
    expect(::setenv("SYSCAPE_TEST_ENV_VAR", "hello_syscape", 1) == 0,
           "setenv must succeed");

    const auto val = syscape::environment::get("SYSCAPE_TEST_ENV_VAR");
    expect(val.has_value(), "get of existing env var must succeed");
    expect(val && *val == "hello_syscape",
           "get of existing env var must return matching value");

    const auto exists = syscape::environment::has("SYSCAPE_TEST_ENV_VAR");
    expect(exists.has_value() && *exists == true,
           "has of existing env var must return true");

    const auto nonexistent =
        syscape::environment::get("SYSCAPE_DEFINITELY_NONEXISTENT_VAR_12345");
    expect(!nonexistent, "get of non-existent env var must fail");
    expect(!nonexistent &&
               nonexistent.error() ==
                   syscape::make_error_code(syscape::errc::not_found),
           "get of non-existent env var must return not_found");

    const auto not_exists =
        syscape::environment::has("SYSCAPE_DEFINITELY_NONEXISTENT_VAR_12345");
    expect(not_exists.has_value() && *not_exists == false,
           "has of non-existent env var must return false");

    const auto empty_name_get = syscape::environment::get("");
    expect(!empty_name_get &&
               empty_name_get.error() ==
                   syscape::make_error_code(syscape::errc::invalid_argument),
           "get with empty name must return invalid_argument");

    const auto empty_name_has = syscape::environment::has("");
    expect(!empty_name_has &&
               empty_name_has.error() ==
                   syscape::make_error_code(syscape::errc::invalid_argument),
           "has with empty name must return invalid_argument");

    // Empty variable value test
    ::setenv("SYSCAPE_TEST_EMPTY_VAR", "", 1);
    const auto empty_val = syscape::environment::get("SYSCAPE_TEST_EMPTY_VAR");
    expect(empty_val.has_value(), "get of empty env var must succeed");
    expect(empty_val && empty_val->empty(),
           "get of empty env var must return empty string");
    const auto empty_exists = syscape::environment::has("SYSCAPE_TEST_EMPTY_VAR");
    expect(empty_exists.has_value() && *empty_exists == true,
           "has of empty env var must return true");
    ::unsetenv("SYSCAPE_TEST_EMPTY_VAR");

    // Invalid variable name with '='
    const auto equals_get = syscape::environment::get("FOO=BAR");
    expect(!equals_get &&
               equals_get.error() ==
                   syscape::make_error_code(syscape::errc::invalid_argument),
           "get with '=' in name must return invalid_argument");

    // Invalid UTF-8 variable value
    const char invalid_utf8_val[] = {'\xff', '\xfe', '\0'};
    ::setenv("SYSCAPE_TEST_INVALID_UTF8", invalid_utf8_val, 1);
    const auto invalid_utf8_get =
        syscape::environment::get("SYSCAPE_TEST_INVALID_UTF8");
    expect(!invalid_utf8_get &&
               invalid_utf8_get.error() ==
                   syscape::make_error_code(syscape::errc::invalid_encoding),
           "get with invalid UTF-8 value must return invalid_encoding");
    const auto invalid_utf8_exists =
        syscape::environment::has("SYSCAPE_TEST_INVALID_UTF8");
    expect(invalid_utf8_exists && *invalid_utf8_exists,
           "has must report an existing variable independently of its value encoding");
    ::unsetenv("SYSCAPE_TEST_INVALID_UTF8");

    // Test environment_variables() snapshot
    const auto vars = syscape::environment::environment_variables();
    expect(vars.has_value(), "environment_variables snapshot must succeed");
    expect(vars && !vars->empty(), "environment_variables snapshot must not be empty");
    if (vars) {
        bool found_test_var = false;
        bool is_sorted = true;
        for (std::size_t i = 0; i < vars->size(); ++i) {
            if ((*vars)[i].name == "SYSCAPE_TEST_ENV_VAR") {
                found_test_var = true;
                expect((*vars)[i].value == "hello_syscape",
                       "snapshot value for SYSCAPE_TEST_ENV_VAR must match");
            }
            if (i > 0 && (*vars)[i - 1].name > (*vars)[i].name) {
                is_sorted = false;
            }
        }
        expect(found_test_var, "snapshot must contain SYSCAPE_TEST_ENV_VAR");
        expect(is_sorted, "snapshot must be lexicographically sorted by name");

        // Test comparison operators
        if (!vars->empty()) {
            const syscape::environment::environment_variable copy = (*vars)[0];
            expect(copy == (*vars)[0], "environment_variable copy equality must hold");
            expect(!(copy != (*vars)[0]), "environment_variable copy inequality must be false");
            syscape::environment::environment_variable modified = copy;
            modified.value += "_diff";
            expect(copy != modified, "different value must not be equal");
        }
    }

    ::unsetenv("SYSCAPE_TEST_ENV_VAR");
}

void test_current_working_directory() {
    const auto cwd = syscape::environment::current_working_directory();
    expect(cwd.has_value(), "current_working_directory must succeed");
    expect(cwd && !cwd->empty() && cwd->front() == '/',
           "current_working_directory on Linux must be an absolute path");

    char buf[4096];
    if (::getcwd(buf, sizeof(buf)) != nullptr) {
        expect(cwd && *cwd == buf,
               "current_working_directory must match getcwd");
    }
}

void test_find_executable() {
    // Standard commands
    const auto sh_path = syscape::environment::find_executable("sh");
    expect(sh_path.has_value(), "find_executable('sh') must succeed");
    expect(sh_path && !sh_path->empty() && sh_path->front() == '/',
           "find_executable('sh') must return an absolute path");

    const auto ls_path = syscape::environment::find_executable("ls");
    expect(ls_path.has_value(), "find_executable('ls') must succeed");

    // Non-existent command
    const auto nonexistent =
        syscape::environment::find_executable("syscape_nonexistent_binary_xyz123");
    expect(!nonexistent, "find_executable of non-existent binary must fail");
    expect(!nonexistent &&
               nonexistent.error() ==
                   syscape::make_error_code(syscape::errc::not_found),
           "find_executable of non-existent binary must return not_found");

    // Direct path lookup with slash
    if (sh_path) {
        const auto direct_sh = syscape::environment::find_executable(*sh_path);
        expect(direct_sh.has_value() && *direct_sh == *sh_path,
               "find_executable with explicit path must resolve correctly");
    }

    // Directory lookup (not an executable)
    const auto dir_lookup = syscape::environment::find_executable("/tmp");
    expect(!dir_lookup &&
               dir_lookup.error() ==
                   syscape::make_error_code(syscape::errc::not_found),
           "find_executable on directory must return not_found");

    // Invalid arguments
    const auto empty_name = syscape::environment::find_executable("");
    expect(!empty_name &&
               empty_name.error() ==
                   syscape::make_error_code(syscape::errc::invalid_argument),
           "find_executable with empty name must return invalid_argument");

    const auto null_name =
        syscape::environment::find_executable(std::string_view("a\0b", 3));
    expect(!null_name &&
               null_name.error() ==
                   syscape::make_error_code(syscape::errc::invalid_argument),
           "find_executable with embedded null must return invalid_argument");

    const char invalid_utf8[] = {'\xff', '\xfe', '\0'};
    const auto invalid_utf8_name =
        syscape::environment::find_executable(invalid_utf8);
    expect(!invalid_utf8_name &&
               invalid_utf8_name.error() ==
                   syscape::make_error_code(syscape::errc::invalid_encoding),
           "find_executable with invalid UTF-8 must return invalid_encoding");

    // Test relative path resolution returning absolute normalized path
    const auto rel_self = syscape::environment::find_executable("./tests_environment_linux");
    if (rel_self) {
        expect(!rel_self->empty() && rel_self->front() == '/',
               "find_executable on relative path must return absolute path");
        expect(rel_self->find("/./") == std::string::npos,
               "find_executable returned path must not contain /./");
    }

    // Test PATH search with trailing and leading colons
    const char* old_path = ::getenv("PATH");
    const std::string saved_path = old_path != nullptr ? old_path : "";

    ::setenv("PATH", "/usr/bin:/bin:", 1);
    const auto sh_trailing = syscape::environment::find_executable("sh");
    expect(sh_trailing.has_value(), "find_executable with trailing colon in PATH must succeed");
    expect(sh_trailing && !sh_trailing->empty() && sh_trailing->front() == '/',
           "find_executable with trailing colon in PATH must return absolute path");

    ::setenv("PATH", ":/usr/bin:/bin", 1);
    const auto sh_leading = syscape::environment::find_executable("sh");
    expect(sh_leading.has_value(), "find_executable with leading colon in PATH must succeed");
    expect(sh_leading && !sh_leading->empty() && sh_leading->front() == '/',
           "find_executable with leading colon in PATH must return absolute path");

    if (!saved_path.empty()) {
        ::setenv("PATH", saved_path.c_str(), 1);
    } else {
        ::unsetenv("PATH");
    }
}

void test_separators() {
    expect(syscape::environment::path_list_separator() == ':',
           "path_list_separator on Linux must be ':'");
    expect(syscape::environment::directory_separator() == '/',
           "directory_separator on Linux must be '/'");
}

void test_standard_directories() {
    const auto tmp = syscape::environment::temp_directory();
    expect(tmp.has_value(), "temp_directory must succeed");
    expect(tmp && !tmp->empty() && tmp->front() == '/',
           "temp_directory on Linux must return an absolute path");

    const auto home = syscape::environment::home_directory();
    expect(home.has_value(), "home_directory must succeed");
    expect(home && !home->empty() && home->front() == '/',
           "home_directory on Linux must return an absolute path");

    const auto config = syscape::environment::config_directory();
    expect(config.has_value(), "config_directory must succeed");
    expect(config && !config->empty() && config->front() == '/',
           "config_directory on Linux must return an absolute path");

    const auto data = syscape::environment::data_directory();
    expect(data.has_value(), "data_directory must succeed");
    expect(data && !data->empty() && data->front() == '/',
           "data_directory on Linux must return an absolute path");

    const auto cache = syscape::environment::cache_directory();
    expect(cache.has_value(), "cache_directory must succeed");
    expect(cache && !cache->empty() && cache->front() == '/',
           "cache_directory on Linux must return an absolute path");
}

void test_terminal_detection() {
    const auto stdin_is_atty = syscape::environment::is_interactive_stdin();
    expect(stdin_is_atty.has_value(), "is_interactive_stdin query must succeed");

    const auto stdout_is_atty = syscape::environment::is_interactive_stdout();
    expect(stdout_is_atty.has_value(),
           "is_interactive_stdout query must succeed");

    const auto stderr_is_atty = syscape::environment::is_interactive_stderr();
    expect(stderr_is_atty.has_value(),
           "is_interactive_stderr query must succeed");
}

void test_common_helpers() {
    using namespace syscape::detail::environment_common;
    expect(!validate_variable_name("").has_value(), "empty string is invalid env name");
    expect(!validate_variable_name("A=B").has_value(), "string with '=' is invalid env name");
    expect(!validate_variable_name(std::string_view("A\0B", 3)).has_value(),
           "string with embedded null is invalid env name");
    expect(validate_variable_name("PATH").has_value(), "PATH is valid env name");
    expect(validate_variable_name("MY_VAR_123").has_value(), "MY_VAR_123 is valid env name");

    const std::string trimmed =
        normalize_directory_path(std::string("/foo/bar///"));
    expect(trimmed == "/foo/bar", "trailing slashes must be stripped");

    const std::string single_slash =
        normalize_directory_path(std::string("////"));
    expect(single_slash == "/", "root slash must be preserved as single slash");

    const std::string empty_slash =
        normalize_directory_path(std::string(""));
    expect(empty_slash.empty(), "empty string remains empty");
}

} // namespace

int main() {
    test_common_helpers();
    test_separators();
    test_environment_variables();
    test_current_working_directory();
    test_find_executable();
    test_standard_directories();
    test_terminal_detection();

    if (failures != 0) {
        std::cerr << failures << " test failure(s) detected.\n";
        return 1;
    }
    return 0;
}
