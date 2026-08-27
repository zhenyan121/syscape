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

    const std::wstring lone_surrogate(1U, static_cast<wchar_t>(0xD800U));
    const auto surrogate_wide =
        syscape::detail::user_backend::wide_to_utf8(lone_surrogate);
    if (surrogate_wide ||
        surrogate_wide.error() != syscape::errc::invalid_encoding) {
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
    if (invalid_token ||
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

    // FILETIME conversion tests
    ::FILETIME zero_ft {};
    // Pre-1970 timestamp must fail as malformed_data
    const auto zero_res =
        syscape::detail::user_backend::filetime_to_time_point(zero_ft);
    if (zero_res || zero_res.error() != syscape::errc::malformed_data) {
        return 14;
    }
    // Unix epoch 1970-01-01 00:00:00 UTC in Windows FILETIME: 116444736000000000
    ::FILETIME epoch_ft {};
    epoch_ft.dwLowDateTime = static_cast<::DWORD>(116444736000000000ULL & 0xFFFFFFFFULL);
    epoch_ft.dwHighDateTime = static_cast<::DWORD>(116444736000000000ULL >> 32U);
    const auto epoch_tp = syscape::detail::user_backend::filetime_to_time_point(epoch_ft);
    if (!epoch_tp.has_value() ||
        epoch_tp->time_since_epoch() != std::chrono::system_clock::duration::zero()) {
        return 15;
    }
    // Maximum/overflow FILETIME must fail as value_too_large
    ::FILETIME overflow_ft {};
    overflow_ft.dwLowDateTime = 0xFFFFFFFFU;
    overflow_ft.dwHighDateTime = 0xFFFFFFFFU;
    const auto overflow_res =
        syscape::detail::user_backend::filetime_to_time_point(overflow_ft);
    if (overflow_res || overflow_res.error() != syscape::errc::value_too_large) {
        return 16;
    }

    // Bounded wide string tests
    const wchar_t test_str[] = L"hello";
    const auto bounded_ok =
        syscape::detail::user_backend::extract_bounded_wide_string(
            test_str, sizeof(test_str));
    if (!bounded_ok || *bounded_ok != L"hello") {
        return 17;
    }
    // Misaligned byte count must fail as malformed_data
    const auto bounded_misaligned =
        syscape::detail::user_backend::extract_bounded_wide_string(
            test_str, 3U);
    if (bounded_misaligned ||
        bounded_misaligned.error() != syscape::errc::malformed_data) {
        return 18;
    }

    // Unterminated wide string buffer
    const wchar_t unterminated_str[2] = {L'a', L'b'};
    const auto bounded_unterminated =
        syscape::detail::user_backend::extract_bounded_wide_string(
            unterminated_str, sizeof(unterminated_str));
    if (bounded_unterminated ||
        bounded_unterminated.error() != syscape::errc::malformed_data) {
        return 19;
    }

    // Null buffer with non-zero bytes
    const auto bounded_null_nonzero =
        syscape::detail::user_backend::extract_bounded_wide_string(
            nullptr, 10U);
    if (bounded_null_nonzero ||
        bounded_null_nonzero.error() != syscape::errc::malformed_data) {
        return 20;
    }

    // Non-null buffer with zero bytes
    const auto bounded_nonnull_zero =
        syscape::detail::user_backend::extract_bounded_wide_string(
            test_str, 0U);
    if (bounded_nonnull_zero ||
        bounded_nonnull_zero.error() != syscape::errc::malformed_data) {
        return 21;
    }

    // Synthetic WTS session parsing tests
    ::WTS_SESSION_INFOW fake_sessions[3] {};
    fake_sessions[0].SessionId = 0;
    fake_sessions[0].pWinStationName = const_cast<wchar_t*>(L"Services");
    fake_sessions[0].State = WTSConnected;

    fake_sessions[1].SessionId = 1;
    fake_sessions[1].pWinStationName = const_cast<wchar_t*>(L"Console");
    fake_sessions[1].State = WTSActive;

    fake_sessions[2].SessionId = 2;
    fake_sessions[2].pWinStationName = const_cast<wchar_t*>(L"RDP-Tcp#1");
    fake_sessions[2].State = WTSDisconnected;

    wchar_t user_empty[] = L"";
    wchar_t user_alice[] = L"alice";
    wchar_t user_bob[] = L"bob";
    wchar_t client_name[] = L"DESKTOP-CLIENT";

    const auto parsed = syscape::detail::user_backend::parse_wts_sessions_impl(
        [&](::HANDLE, ::DWORD, ::DWORD, ::PWTS_SESSION_INFOW* ppSessionInfo,
            ::DWORD* pCount) -> ::BOOL {
            *ppSessionInfo = fake_sessions;
            *pCount = 3;
            return TRUE;
        },
        [&](::HANDLE, ::DWORD sessionId, ::WTS_INFO_CLASS infoClass,
            ::LPWSTR* ppBuffer, ::DWORD* pBytesReturned) -> ::BOOL {
            if (infoClass == WTSUserName) {
                if (sessionId == 0) {
                    *ppBuffer = user_empty;
                    *pBytesReturned = sizeof(user_empty);
                } else if (sessionId == 1) {
                    *ppBuffer = user_alice;
                    *pBytesReturned = sizeof(user_alice);
                } else if (sessionId == 2) {
                    *ppBuffer = user_bob;
                    *pBytesReturned = sizeof(user_bob);
                }
                return TRUE;
            }
            if (infoClass == WTSClientName && sessionId == 2) {
                *ppBuffer = client_name;
                *pBytesReturned = sizeof(client_name);
                return TRUE;
            }
            return FALSE;
        },
        [](void*) {});

    if (!parsed || parsed->size() != 2U) {
        return 22;
    }

    if ((*parsed)[0].user_name != "alice" ||
        (*parsed)[0].terminal != "Console" ||
        (*parsed)[0].type != syscape::user::session_type::graphical ||
        (*parsed)[0].state != syscape::user::session_state::active ||
        (*parsed)[0].remote_host.has_value()) {
        return 23;
    }

    if ((*parsed)[1].user_name != "bob" ||
        (*parsed)[1].terminal != "RDP-Tcp#1" ||
        (*parsed)[1].type != syscape::user::session_type::remote ||
        (*parsed)[1].state != syscape::user::session_state::disconnected ||
        !(*parsed)[1].remote_host.has_value() ||
        *(*parsed)[1].remote_host != "DESKTOP-CLIENT") {
        return 24;
    }

    const auto users = syscape::detail::user_common::extract_logged_in_users(parsed);
    if (!users || users->size() != 2U || (*users)[0] != "alice" || (*users)[1] != "bob") {
        return 25;
    }

    // UTF-16 encoding error propagation test in WTSUserName
    wchar_t lone_surrogate_user[2] = {static_cast<wchar_t>(0xD800U), L'\0'};
    ::WTS_SESSION_INFOW surrogate_session {};
    surrogate_session.SessionId = 1;
    surrogate_session.pWinStationName = const_cast<wchar_t*>(L"Console");
    surrogate_session.State = WTSActive;

    const auto surrogate_parsed = syscape::detail::user_backend::parse_wts_sessions_impl(
        [&](::HANDLE, ::DWORD, ::DWORD, ::PWTS_SESSION_INFOW* ppSessionInfo,
            ::DWORD* pCount) -> ::BOOL {
            *ppSessionInfo = &surrogate_session;
            *pCount = 1;
            return TRUE;
        },
        [&](::HANDLE, ::DWORD, ::WTS_INFO_CLASS infoClass,
            ::LPWSTR* ppBuffer, ::DWORD* pBytesReturned) -> ::BOOL {
            if (infoClass == WTSUserName) {
                *ppBuffer = lone_surrogate_user;
                *pBytesReturned = sizeof(lone_surrogate_user);
                return TRUE;
            }
            return FALSE;
        },
        [](void*) {});

    if (surrogate_parsed ||
        surrogate_parsed.error() != syscape::errc::invalid_encoding) {
        return 26;
    }

    // Truncated WTS_CLIENT_ADDRESS buffer must report malformed_data
    ::WTS_CLIENT_ADDRESS fake_addr {};
    const auto truncated_addr_parsed = syscape::detail::user_backend::parse_wts_sessions_impl(
        [&](::HANDLE, ::DWORD, ::DWORD, ::PWTS_SESSION_INFOW* ppSessionInfo,
            ::DWORD* pCount) -> ::BOOL {
            *ppSessionInfo = fake_sessions + 1; // session 1
            *pCount = 1;
            return TRUE;
        },
        [&](::HANDLE, ::DWORD, ::WTS_INFO_CLASS infoClass,
            ::LPWSTR* ppBuffer, ::DWORD* pBytesReturned) -> ::BOOL {
            if (infoClass == WTSUserName) {
                *ppBuffer = user_alice;
                *pBytesReturned = sizeof(user_alice);
                return TRUE;
            }
            if (infoClass == WTSClientAddress) {
                *ppBuffer = reinterpret_cast<::LPWSTR>(&fake_addr);
                *pBytesReturned = sizeof(::WTS_CLIENT_ADDRESS) - 1U; // truncated!
                return TRUE;
            }
            return FALSE;
        },
        [](void*) {});

    if (truncated_addr_parsed ||
        truncated_addr_parsed.error() != syscape::errc::malformed_data) {
        return 27;
    }

    // WTSUserName query failure must propagate native system error
    const auto fail_query_parsed = syscape::detail::user_backend::parse_wts_sessions_impl(
        [&](::HANDLE, ::DWORD, ::DWORD, ::PWTS_SESSION_INFOW* ppSessionInfo,
            ::DWORD* pCount) -> ::BOOL {
            *ppSessionInfo = fake_sessions + 1;
            *pCount = 1;
            return TRUE;
        },
        [&](::HANDLE, ::DWORD, ::WTS_INFO_CLASS, ::LPWSTR*, ::DWORD*) -> ::BOOL {
            ::SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        },
        [](void*) {});

    if (fail_query_parsed ||
        fail_query_parsed.error() != std::errc::permission_denied) {
        return 28;
    }

    return 0;
}
