#include <cstdint>
#include <string>
#include <vector>

#include <syscape/filesystem.hpp>
#include <syscape/filesystem.hpp>

int main() {
    const syscape::result<std::vector<syscape::filesystem::mount_entry>>
        mounted = syscape::filesystem::mounts();
    const syscape::result<syscape::filesystem::space_info> capacity =
        syscape::filesystem::space("/");
    const syscape::result<syscape::filesystem::path_length_limit>
        component = syscape::filesystem::max_component_length("/");
    const syscape::result<syscape::filesystem::path_length_limit> whole =
        syscape::filesystem::max_path_length("/");
    const syscape::result<std::string> identifier =
        syscape::filesystem::volume_id("/");

    static_cast<void>(mounted);
    static_cast<void>(component);
    static_cast<void>(whole);
    static_cast<void>(identifier);
    return capacity && capacity->block_size_bytes == 0U ? 1 : 0;
}
