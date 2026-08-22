#include <cstdint>
#include <string>
#include <vector>

#include <syscape/process.hpp>
#include <syscape/process.hpp>

int main() {
    const syscape::result<std::uint32_t> id =
        syscape::process::process_id();
    const syscape::result<std::uint32_t> parent =
        syscape::process::parent_process_id();
    const syscape::result<std::string> path =
        syscape::process::executable_path();
    const syscape::result<std::vector<std::string>> arguments =
        syscape::process::command_line();
    const syscape::result<std::string> directory =
        syscape::process::working_directory();

    static_cast<void>(parent);
    static_cast<void>(path);
    static_cast<void>(arguments);
    static_cast<void>(directory);
    return id && *id == 0U ? 1 : 0;
}
