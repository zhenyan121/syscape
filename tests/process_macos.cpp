#include <cstdint>
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
    expect(id && *id > 0U, "macOS must report a positive process ID");

    const auto parent = syscape::process::parent_process_id();
    expect(parent.has_value(), "macOS must query the parent process ID");

    const auto executable = syscape::process::executable_path();
    expect(executable && !executable->empty() &&
               executable->front() == '/' &&
               syscape::detail::is_valid_utf8(*executable),
           "macOS must report an absolute UTF-8 executable path");

    const auto arguments = syscape::process::command_line();
    expect(arguments && !arguments->empty(),
           "macOS must expose _NSGetArgv and _NSGetArgc as a nonempty list");

    const auto working_directory = syscape::process::working_directory();
    expect(working_directory && !working_directory->empty() &&
               working_directory->front() == '/',
           "macOS must report an absolute working directory");

    return failures == 0 ? 0 : 1;
}
