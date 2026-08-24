#include <system_error>
#include <vector>

#include <syscape/network.hpp>

int main() {
    const syscape::result<std::vector<syscape::network::interface_entry>>
        interfaces = syscape::network::interfaces();
    const auto routes = syscape::network::routes();
    const auto gateways = syscape::network::default_gateways();
    const auto dns = syscape::network::dns();
    return !interfaces && !routes && !gateways && !dns &&
                   interfaces.error() == std::errc::operation_not_supported &&
                   routes.error() == std::errc::operation_not_supported &&
                   gateways.error() == std::errc::operation_not_supported &&
                   dns.error() == std::errc::operation_not_supported
               ? 0
               : 1;
}
