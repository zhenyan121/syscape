#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <sys/stat.h>
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
    expect(cwd && !cwd->empty() && cwd->front() == '/',
           "current working directory must be an absolute path");

    const auto tmp = syscape::environment::temp_directory();
    expect((tmp && !tmp->empty() && tmp->front() == '/') ||
               tmp.error() == syscape::errc::not_found,
           "temp directory must be an absolute existing path or not_found");

    const auto home = syscape::environment::home_directory();
    expect(home.has_value() || home.error() == syscape::errc::not_found,
           "home directory query must succeed or report not_found");

    const auto config = syscape::environment::config_directory();
    expect(!config && config.error() == syscape::errc::not_supported,
           "config directory must report not_supported on NuttX");

    const auto data = syscape::environment::data_directory();
    expect(!data && data.error() == syscape::errc::not_supported,
           "data directory must report not_supported on NuttX");

    const auto cache = syscape::environment::cache_directory();
    expect(!cache && cache.error() == syscape::errc::not_supported,
           "cache directory must report not_supported on NuttX");

    const auto vars = syscape::environment::environment_variables();
    expect(vars.has_value(), "environment variables query must succeed");
}

void test_find_executable() {
    const auto sh_path = syscape::environment::find_executable("sh");
    expect(sh_path.has_value(), "find_executable('sh') must succeed");
    if (sh_path) {
        expect(!sh_path->empty() && sh_path->front() == '/',
               "find_executable('sh') must return an absolute path");
    }

    const auto direct_sh = syscape::environment::find_executable("/bin/sh");
    expect(direct_sh.has_value(),
           "find_executable with explicit path must resolve correctly");

    const auto nonexistent = syscape::environment::find_executable(
        "syscape_nonexistent_binary_xyz123");
    expect(!nonexistent && nonexistent.error() == syscape::errc::not_found,
           "find_executable of non-existent binary must return not_found");

    const auto empty_name = syscape::environment::find_executable("");
    expect(!empty_name && empty_name.error() == syscape::errc::invalid_argument,
           "find_executable with empty name must return invalid_argument");

    const auto embedded_null =
        syscape::environment::find_executable(std::string_view("a\0b", 3));
    expect(!embedded_null &&
               embedded_null.error() == syscape::errc::invalid_argument,
           "find_executable with embedded null must return invalid_argument");

    const char invalid_utf8[] = {'\xff', '\xfe', '\0'};
    const auto invalid_utf8_name =
        syscape::environment::find_executable(invalid_utf8);
    expect(
        !invalid_utf8_name &&
            invalid_utf8_name.error() == syscape::errc::invalid_encoding,
        "find_executable with invalid UTF-8 name must return invalid_encoding");

    // Test non-UTF-8 PATH directory containing executable
    const char invalid_dir[] = "/tmp/syscape_test_nuttx_\xff\xfe";
    if (::mkdir(invalid_dir, 0755) == 0) {
        std::string test_bin = std::string(invalid_dir) + "/test_bin";
        std::FILE* fp = std::fopen(test_bin.c_str(), "w");
        if (fp != nullptr) {
            std::fputs("#!/bin/sh\nexit 0\n", fp);
            std::fclose(fp);
            ::chmod(test_bin.c_str(), 0755);

            const char* old_path = ::getenv("PATH");
            std::string saved_path = old_path != nullptr ? old_path : "";
            const bool had_path = (old_path != nullptr);

            ::setenv("PATH", invalid_dir, 1);
            const auto result =
                syscape::environment::find_executable("test_bin");
            expect(!result && result.error() == syscape::errc::invalid_encoding,
                   "find_executable in non-UTF-8 PATH directory must return "
                   "invalid_encoding");

            if (had_path) {
                ::setenv("PATH", saved_path.c_str(), 1);
            } else {
                ::unsetenv("PATH");
            }
            ::unlink(test_bin.c_str());
        }
        ::rmdir(invalid_dir);
    }
}

} // namespace

int main() {
    test_environment_queries();
    test_find_executable();
    return failures == 0 ? 0 : 1;
}
