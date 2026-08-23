#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

#include <syscape/detail/environment/common.hpp>
#include <syscape/detail/environment/linux.hpp>
#include <syscape/detail/environment/posix.hpp>
#include <syscape/environment.hpp>

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

    ::unsetenv("SYSCAPE_TEST_ENV_VAR");
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
    test_environment_variables();
    test_standard_directories();
    test_terminal_detection();

    if (failures != 0) {
        std::cerr << failures << " test failure(s) detected.\n";
        return 1;
    }
    return 0;
}
