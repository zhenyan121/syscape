#include <system_error>
#include <vector>

#include <syscape/network.hpp>

int main() {
    const syscape::result<std::vector<syscape::network::interface_entry>>
        interfaces = syscape::network::interfaces();
    return !interfaces &&
                   interfaces.error() == std::errc::operation_not_supported
               ? 0
               : 1;
}
