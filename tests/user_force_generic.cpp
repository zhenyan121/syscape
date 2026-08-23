#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

#include <syscape/user.hpp>

template <typename T>
bool unsupported(const syscape::result<T>& value) {
    return !value && value.error() == std::errc::operation_not_supported;
}

int main() {
    return unsupported(syscape::user::real_user_id()) &&
                   unsupported(syscape::user::effective_user_id()) &&
                   unsupported(syscape::user::real_group_id()) &&
                   unsupported(syscape::user::effective_group_id()) &&
                   unsupported(syscape::user::supplementary_groups()) &&
                   unsupported(syscape::user::privilege()) &&
                   unsupported(syscape::user::user_name()) &&
                   unsupported(syscape::user::login_name()) &&
                   unsupported(syscape::user::home_directory()) &&
                   unsupported(syscape::user::shell())
               ? 0
               : 1;
}
