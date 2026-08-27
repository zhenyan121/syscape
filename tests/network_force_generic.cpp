#include <system_error>
#include <vector>

#include <syscape/network.hpp>

int main() {
    const syscape::result<std::vector<syscape::network::interface_entry>>
        interfaces = syscape::network::interfaces();
    const auto routes = syscape::network::routes();
    const auto gateways = syscape::network::default_gateways();
    const auto dns = syscape::network::dns();
    const auto stats = syscape::network::statistics();
    const auto stats_name = syscape::network::statistics("lo");
    const auto stats_index = syscape::network::statistics(1U);
    return !interfaces && !routes && !gateways && !dns && !stats &&
                   !stats_name && !stats_index &&
                   interfaces.error() == std::errc::operation_not_supported &&
                   routes.error() == std::errc::operation_not_supported &&
                   gateways.error() == std::errc::operation_not_supported &&
                   dns.error() == std::errc::operation_not_supported &&
                   stats.error() == std::errc::operation_not_supported &&
                   stats_name.error() == std::errc::operation_not_supported &&
                   stats_index.error() == std::errc::operation_not_supported
               ? 0
               : 1;
}
