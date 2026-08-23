#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

#include <syscape/detail/user/windows.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/user.hpp>

namespace {

template <typename T>
bool unsupported(const syscape::result<T>& value) {
    return !value && value.error() == std::errc::operation_not_supported;
}

} // namespace

int main() {
    const auto ascii =
        syscape::detail::user_backend::wide_to_utf8(L"profile");
    if (!ascii || *ascii != "profile") { return 1; }

    const auto supplementary = syscape::detail::user_backend::wide_to_utf8(
        std::wstring(1U, static_cast<wchar_t>(0x00E9U)));
    if (!supplementary || *supplementary != "\xC3\xA9") { return 2; }

    std::wstring lone_surrogate(1U, static_cast<wchar_t>(0xD800U));
    if (syscape::detail::user_backend::wide_to_utf8(lone_surrogate)
            .error() != syscape::errc::invalid_encoding) {
        return 3;
    }

    const auto denied = syscape::detail::user_backend::map_hresult(
        ::HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED));
    if (!(denied == std::errc::permission_denied)) { return 4; }

    const auto out_of_memory =
        syscape::detail::user_backend::map_hresult(E_OUTOFMEMORY);
    if (out_of_memory != std::error_code(
                             static_cast<int>(ERROR_OUTOFMEMORY),
                             std::system_category())) {
        return 5;
    }

    const auto generic_failure =
        syscape::detail::user_backend::map_hresult(E_FAIL);
    if (generic_failure.value() != static_cast<int>(E_FAIL) ||
        generic_failure.message().empty() ||
        &generic_failure.category() !=
            &syscape::detail::user_backend::hresult_category()) {
        return 6;
    }

    if (!syscape::detail::user_backend::is_absolute_home_path(
            L"C:\\Users\\alice") ||
        syscape::detail::user_backend::is_absolute_home_path(
            L"Users\\alice")) {
        return 7;
    }

    if (!unsupported(syscape::user::real_user_id()) ||
        !unsupported(syscape::user::effective_user_id()) ||
        !unsupported(syscape::user::real_group_id()) ||
        !unsupported(syscape::user::effective_group_id()) ||
        !unsupported(syscape::user::supplementary_groups()) ||
        !unsupported(syscape::user::login_name()) ||
        !unsupported(syscape::user::shell())) {
        return 8;
    }

    // An invalid token handle exercises the documented native failure path
    // of the token-elevation query without requiring elevated permissions.
    const auto invalid_token =
        syscape::detail::user_backend::classify_token_elevation(nullptr);
    if (!invalid_token ||
        invalid_token.error().category() != std::system_category()) {
        return 9;
    }

    // The current process token query succeeds on every supported Windows
    // release; sandboxed environments that deny access must preserve their
    // native system error instead of a portable placeholder.
    const auto privilege = syscape::user::privilege();
    if (!privilege &&
        privilege.error().category() != std::system_category()) {
        return 10;
    }
    if (privilege &&
        *privilege != syscape::user::privilege_state::privileged &&
        *privilege != syscape::user::privilege_state::unprivileged) {
        return 11;
    }

    const auto name = syscape::user::user_name();
    if (!name || name->empty() ||
        !syscape::detail::is_valid_utf8(*name)) {
        return 12;
    }

    const auto home = syscape::user::home_directory();
    if (!home || home->empty() || !syscape::detail::is_valid_utf8(*home)) {
        return 13;
    }

    return 0;
}
