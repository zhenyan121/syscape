#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include <syscape/detail/process/linux.hpp>
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

void test_command_line_parser() {
    constexpr std::string_view simple_input("one\0two\0", 8U);
    const auto simple =
        syscape::detail::process_backend::parse_command_line(simple_input);
    expect(simple && simple->size() == 2U && (*simple)[0] == "one" &&
               (*simple)[1] == "two",
           "NUL-separated command-line values must be preserved");

    constexpr std::string_view empty_value_input("first\0\0third\0", 13U);
    const auto empty_value =
        syscape::detail::process_backend::parse_command_line(
            empty_value_input);
    expect(empty_value && empty_value->size() == 3U &&
               (*empty_value)[1].empty(),
           "Empty command-line arguments are valid and must not be dropped");

    constexpr std::string_view one_empty_input("\0", 1U);
    const auto only_empty =
        syscape::detail::process_backend::parse_command_line(one_empty_input);
    expect(only_empty && only_empty->size() == 1U &&
               (*only_empty)[0].empty(),
           "An empty argv[0] is valid data");

    const auto empty_source =
        syscape::detail::process_backend::parse_command_line("");
    expect(empty_source && empty_source->empty(),
           "An empty command-line source represents an empty argument list");

    const auto missing_terminator =
        syscape::detail::process_backend::parse_command_line("broken");
    expect(!missing_terminator &&
               missing_terminator.error() ==
                   syscape::errc::malformed_data,
           "Command-line data without a final NUL must be malformed");

    const auto missing_last_terminator =
        syscape::detail::process_backend::parse_command_line("one\0two");
    expect(!missing_last_terminator &&
               missing_last_terminator.error() ==
                   syscape::errc::malformed_data,
           "The final command-line value still requires a terminating NUL");

    std::vector<std::string> invalid_encoding;
    invalid_encoding.emplace_back(1U, static_cast<char>(0xff));
    const auto invalid_text =
        syscape::detail::process_common::validate_utf8_arguments(
            syscape::result<std::vector<std::string>>(invalid_encoding));
    expect(!invalid_text &&
               invalid_text.error() == syscape::errc::invalid_encoding,
           "Invalid argument text must fail at the public boundary");
}

struct fake_read_link {
    std::string value;
    bool interrupted = true;

    ssize_t operator()(char* buffer, std::size_t size) {
        if (interrupted) {
            interrupted = false;
            errno = EINTR;
            return -1;
        }
        const std::size_t count = value.size() < size ? value.size() : size;
        value.copy(buffer, count);
        return static_cast<ssize_t>(count);
    }
};

void test_path_buffer_growth() {
    fake_read_link operation{"", false};
    // Start with enough bytes to force several doublings from the internal
    // 256-byte initial buffer without depending on PATH_MAX.
    constexpr std::size_t target_length = 700U;
    operation.value.resize(target_length, 'x');
    operation.value.front() = '/';

    const auto path = syscape::detail::process_backend::read_link_with_growth(
        operation);
    expect(path && path->size() == target_length,
           "Executable-path reads must grow truncated buffers");

    fake_read_link interrupted_operation{"/executable", true};
    const auto retried =
        syscape::detail::process_backend::read_link_with_growth(
            interrupted_operation);
    expect(retried && *retried == "/executable",
           "Interrupted executable-path reads must be retried");
}

void test_runtime_queries() {
    const auto id = syscape::process::process_id();
    expect(id && *id > 0U,
           "Linux must report a positive process ID from getpid()");

    const auto parent = syscape::process::parent_process_id();
    expect(parent.has_value(),
           "Linux must query the parent process ID from getppid()");

    const auto executable = syscape::process::executable_path();
    expect(executable && !executable->empty() &&
               executable->front() == '/' &&
               syscape::detail::is_valid_utf8(*executable),
           "Linux must report an absolute UTF-8 executable path from "
           "/proc/self/exe");

    const auto command_line = syscape::process::command_line();
    expect(command_line && !command_line->empty(),
           "Linux must report at least one command-line value");
    if (command_line) {
        for (const std::string& value : *command_line) {
            expect(syscape::detail::is_valid_utf8(value),
                   "Every command-line value must be valid UTF-8");
        }
    }

    const auto working_directory = syscape::process::working_directory();
    expect(working_directory && !working_directory->empty() &&
               working_directory->front() == '/' &&
               syscape::detail::is_valid_utf8(*working_directory),
           "Linux must report an absolute UTF-8 working directory from "
           "getcwd()");
}

} // namespace

int main() {
    test_command_line_parser();
    test_path_buffer_growth();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
