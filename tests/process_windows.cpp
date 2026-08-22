#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/process.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

} // namespace

int main() {
    const auto id = syscape::process::process_id();
    expect(id && *id > 0U, "Windows must report a positive process ID");

    const auto parent = syscape::process::parent_process_id();
    expect(parent.has_value(),
           "Windows must locate the current process in the Toolhelp snapshot");

    const auto executable = syscape::process::executable_path();
    expect(executable && !executable->empty() &&
               syscape::detail::is_valid_utf8(*executable),
           "Windows must convert GetModuleFileNameW output to UTF-8");
    if (executable) {
        expect(std::filesystem::path(*executable).is_absolute(),
               "Windows executable paths must be absolute");
    }

    const auto arguments = syscape::process::command_line();
    expect(arguments && !arguments->empty(),
           "Windows must split CommandLineToArgvW output into UTF-8 values");

    const auto working_directory = syscape::process::working_directory();
    expect(working_directory && !working_directory->empty() &&
               std::filesystem::path(*working_directory).is_absolute(),
           "Windows must report an absolute working directory");

    return failures == 0 ? 0 : 1;
}
