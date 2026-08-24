#include <cstdint>
#include <system_error>

#include <syscape/storage.hpp>

int main() {
    const syscape::result<std::vector<syscape::storage::drive_entry>> listed =
        syscape::storage::drives();
    return !listed && listed.error() == std::errc::operation_not_supported
               ? 0
               : 1;
}
