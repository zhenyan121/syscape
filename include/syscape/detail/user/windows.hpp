#ifndef SYSCAPE_DETAIL_USER_WINDOWS_HPP
#define SYSCAPE_DETAIL_USER_WINDOWS_HPP

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0601
#error "syscape/user.hpp requires _WIN32_WINNT >= 0x0601 on Windows"
#endif
#if defined(WINVER) && WINVER < 0x0601
#error "syscape/user.hpp requires WINVER >= 0x0601 on Windows"
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <windows.h>
#include <lmcons.h>
#include <shlobj.h>
#include <wtsapi32.h>

#include <syscape/detail/user/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace user_backend {

inline result<std::string> wide_to_utf8(std::wstring_view value) {
    static_assert(sizeof(wchar_t) == sizeof(char16_t),
                  "The Windows backend requires 16-bit wchar_t");
    std::u16string converted;
    converted.reserve(value.size());
    for (wchar_t unit : value) {
        converted.push_back(static_cast<char16_t>(unit));
    }
    return utf16_to_utf8(converted);
}

inline std::error_code last_error() noexcept {
    return std::error_code(static_cast<int>(::GetLastError()),
                           std::system_category());
}

/// Error category for HRESULT values outside the Win32 facility.
///
/// The system category describes Win32 error codes, so raw non-Win32
/// HRESULTs keep their full value under this dedicated category instead of
/// being misinterpreted.
class hresult_error_category final : public std::error_category {
public:
    const char* name() const noexcept override {
        return "windows-hresult";
    }

    std::string message(int value) const override {
        static const char digits[] = "0123456789ABCDEF";
        const unsigned int raw = static_cast<unsigned int>(value);
        std::string text("Windows HRESULT 0x");
        for (int shift = 28; shift >= 0; shift -= 4) {
            text.push_back(digits[(raw >> shift) & 0xFU]);
        }
        return text;
    }
};

/// Returns the process-wide category for raw non-Win32 HRESULT values.
inline const std::error_category& hresult_category() noexcept {
    static const hresult_error_category category;
    return category;
}

/// Maps an HRESULT to an error code without losing diagnostic accuracy.
///
/// Win32-facility HRESULTs are narrowed to their underlying Win32 code under
/// the system category, which keeps standard error conditions such as
/// permission-denied comparable. Every other facility preserves its complete
/// HRESULT value under the dedicated internal category.
inline std::error_code map_hresult(::HRESULT value) noexcept {
    if (HRESULT_FACILITY(value) == FACILITY_WIN32) {
        return std::error_code(static_cast<int>(HRESULT_CODE(value)),
                               std::system_category());
    }
    return std::error_code(static_cast<int>(value), hresult_category());
}

/// Returns whether a native Windows home-directory path is absolute.
inline bool is_absolute_home_path(std::wstring_view value) {
    return std::filesystem::path(value).is_absolute();
}

class co_task_memory {
public:
    explicit co_task_memory(::PWSTR value) noexcept : value_(value) {}
    co_task_memory(const co_task_memory&) = delete;
    co_task_memory& operator=(const co_task_memory&) = delete;
    ~co_task_memory() {
        if (value_ != nullptr) { ::CoTaskMemFree(value_); }
    }

    ::PWSTR get() const noexcept { return value_; }

private:
    ::PWSTR value_;
};

/// Converts a hundred-nanosecond count to a nanoseconds duration with overflow check.
inline result<std::chrono::nanoseconds> hundred_nanoseconds_to_duration(
    std::uint64_t units) noexcept {
    constexpr std::uint64_t maximum_units = static_cast<std::uint64_t>(
        (std::chrono::nanoseconds::max)().count() / 100U);
    if (units > maximum_units) { return fail(errc::value_too_large); }
    return std::chrono::nanoseconds(units * 100U);
}

/// Converts a wall-clock FILETIME value to a system-clock time point with bounds and epoch validation.
inline result<std::chrono::system_clock::time_point> filetime_to_time_point(
    const ::FILETIME& value) {
    using clock = std::chrono::system_clock;
    // 11644473600 seconds separate the 1601 FILETIME and 1970 Unix epochs.
    constexpr std::uint64_t unix_epoch_offset_units = 116444736000000000ULL;
    const std::uint64_t raw_units =
        (static_cast<std::uint64_t>(value.dwHighDateTime) << 32U) |
        static_cast<std::uint64_t>(value.dwLowDateTime);
    if (raw_units < unix_epoch_offset_units) {
        return fail(errc::malformed_data);
    }
    const result<std::chrono::nanoseconds> since_unix_epoch =
        hundred_nanoseconds_to_duration(raw_units - unix_epoch_offset_units);
    if (!since_unix_epoch) { return fail(since_unix_epoch.error()); }
    return clock::time_point(
        std::chrono::duration_cast<clock::duration>(*since_unix_epoch));
}

/// Extracts a bounded wide string slice from a WTS return buffer.
inline result<std::wstring_view> extract_bounded_wide_string(
    const wchar_t* buffer, ::DWORD bytes) {
    if (buffer == nullptr && bytes == 0U) {
        return std::wstring_view{};
    }
    if (buffer == nullptr || bytes == 0U) {
        return fail(errc::malformed_data);
    }
    if ((bytes % sizeof(wchar_t)) != 0U) {
        return fail(errc::malformed_data);
    }
    const std::size_t max_chars = bytes / sizeof(wchar_t);
    std::size_t len = 0U;
    bool found_null = false;
    while (len < max_chars) {
        if (buffer[len] == L'\0') {
            found_null = true;
            break;
        }
        ++len;
    }
    if (!found_null) {
        return fail(errc::malformed_data);
    }
    return std::wstring_view(buffer, len);
}

/// Maps a Windows Terminal Services connect state to a portable session_state.
inline user_common::session_state map_wts_connect_state(
    ::WTS_CONNECTSTATE_CLASS state) {
    switch (state) {
    case WTSActive:
    case WTSConnected:
    case WTSShadow:
        return user_common::session_state::active;
    case WTSIdle:
        return user_common::session_state::idle;
    case WTSDisconnected:
        return user_common::session_state::disconnected;
    default:
        return user_common::session_state::other;
    }
}

/// Classifies a Windows session type based on station name, remote host, and session ID.
inline user_common::session_type classify_windows_session_type(
    const std::string& terminal,
    const std::optional<std::string>& remote_host,
    std::uint32_t session_id) {
    if (terminal == "Console" || terminal == "console") {
        return user_common::session_type::graphical;
    }
    if (terminal.rfind("RDP-", 0) == 0 || terminal.rfind("rdp-", 0) == 0 ||
        remote_host.has_value()) {
        return user_common::session_type::remote;
    }
    if (session_id == 0) {
        return user_common::session_type::system;
    }
    return user_common::session_type::other;
}

/// Parses active user login sessions from injectable WTS operations.
template <typename EnumerateFn, typename QueryFn, typename FreeFn>
inline result<std::vector<user_common::session_info>> parse_wts_sessions_impl(
    EnumerateFn enumerate, QueryFn query, FreeFn free_mem) {
    ::PWTS_SESSION_INFOW pSessions = nullptr;
    ::DWORD count = 0;
    if (!enumerate(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessions, &count)) {
        return fail(last_error());
    }
    if (pSessions == nullptr && count > 0) {
        return fail(errc::malformed_data);
    }
    struct enum_guard {
        ::PWTS_SESSION_INFOW ptr;
        FreeFn free_fn;
        ~enum_guard() {
            if (ptr != nullptr) { free_fn(ptr); }
        }
    } guard{pSessions, free_mem};

    std::vector<user_common::session_info> results;
    results.reserve(count);

    for (::DWORD i = 0; i < count; ++i) {
        const ::WTS_SESSION_INFOW& wts_session = pSessions[i];
        ::LPWSTR user_buf = nullptr;
        ::DWORD user_bytes = 0;
        if (!query(WTS_CURRENT_SERVER_HANDLE, wts_session.SessionId, WTSUserName,
                   reinterpret_cast<::LPWSTR*>(&user_buf), &user_bytes)) {
            return fail(last_error());
        }
        struct user_guard {
            void* ptr;
            FreeFn free_fn;
            ~user_guard() {
                if (ptr != nullptr) { free_fn(ptr); }
            }
        } u_guard{user_buf, free_mem};

        const auto user_view = extract_bounded_wide_string(user_buf, user_bytes);
        if (!user_view) {
            return fail(user_view.error());
        }
        if (user_view->empty()) {
            continue;
        }
        const auto user_utf8 = wide_to_utf8(*user_view);
        if (!user_utf8) {
            return fail(user_utf8.error());
        }
        if (user_utf8->empty()) {
            continue;
        }

        user_common::session_info session;
        session.user_name = *user_utf8;
        session.session_id = std::to_string(wts_session.SessionId);

        if (wts_session.pWinStationName != nullptr) {
            const auto term_utf8 = wide_to_utf8(wts_session.pWinStationName);
            if (!term_utf8) {
                return fail(term_utf8.error());
            }
            session.terminal = *term_utf8;
        }

        session.state = map_wts_connect_state(wts_session.State);

        std::optional<std::string> client_host;

        // Query Client Name
        ::LPWSTR client_buf = nullptr;
        ::DWORD client_bytes = 0;
        if (query(WTS_CURRENT_SERVER_HANDLE, wts_session.SessionId, WTSClientName,
                  reinterpret_cast<::LPWSTR*>(&client_buf), &client_bytes)) {
            struct client_guard {
                void* ptr;
                FreeFn free_fn;
                ~client_guard() {
                    if (ptr != nullptr) { free_fn(ptr); }
                }
            } c_guard{client_buf, free_mem};
            const auto client_view =
                extract_bounded_wide_string(client_buf, client_bytes);
            if (!client_view) {
                return fail(client_view.error());
            }
            if (!client_view->empty()) {
                const auto client_utf8 = wide_to_utf8(*client_view);
                if (!client_utf8) {
                    return fail(client_utf8.error());
                }
                if (!client_utf8->empty()) {
                    client_host = *client_utf8;
                }
            }
        }

        // Query Client Address if client_host not yet set
        if (!client_host.has_value()) {
            ::PWTS_CLIENT_ADDRESS addr_buf = nullptr;
            ::DWORD addr_bytes = 0;
            if (query(WTS_CURRENT_SERVER_HANDLE, wts_session.SessionId,
                      WTSClientAddress,
                      reinterpret_cast<::LPWSTR*>(&addr_buf), &addr_bytes)) {
                struct addr_guard {
                    void* ptr;
                    FreeFn free_fn;
                    ~addr_guard() {
                        if (ptr != nullptr) { free_fn(ptr); }
                    }
                } a_guard{addr_buf, free_mem};
                if (addr_buf == nullptr && addr_bytes > 0U) {
                    return fail(errc::malformed_data);
                }
                if (addr_buf != nullptr) {
                    if (addr_bytes < sizeof(::WTS_CLIENT_ADDRESS)) {
                        return fail(errc::malformed_data);
                    }
                    // AF_INET has standard constant value 2 across Windows SDKs.
                    constexpr ::DWORD wts_af_inet = 2U;
                    if (addr_buf->AddressFamily == wts_af_inet) {
                        std::string ip =
                            std::to_string(addr_buf->Address[2]) + "." +
                            std::to_string(addr_buf->Address[3]) + "." +
                            std::to_string(addr_buf->Address[4]) + "." +
                            std::to_string(addr_buf->Address[5]);
                        if (ip != "0.0.0.0") {
                            client_host = std::move(ip);
                        }
                    }
                }
            }
        }

        // Query Logon Time via WTSSessionInfo (WTSInfoExLevel1)
        ::PWTSINFOEXW info_buf = nullptr;
        ::DWORD info_bytes = 0;
        if (query(WTS_CURRENT_SERVER_HANDLE, wts_session.SessionId,
                  static_cast<::WTS_INFO_CLASS>(25) /* WTSSessionInfoEx */,
                  reinterpret_cast<::LPWSTR*>(&info_buf), &info_bytes)) {
            struct info_guard {
                void* ptr;
                FreeFn free_fn;
                ~info_guard() {
                    if (ptr != nullptr) { free_fn(ptr); }
                }
            } i_guard{info_buf, free_mem};
            if (info_buf == nullptr && info_bytes > 0U) {
                return fail(errc::malformed_data);
            }
            if (info_buf != nullptr) {
                if (info_bytes < sizeof(::WTSINFOEXW)) {
                    return fail(errc::malformed_data);
                }
                if (info_buf->Level == 1) {
                    const auto& l1 = info_buf->Data.WTSInfoExLevel1;
                    ::FILETIME ft;
                    ft.dwLowDateTime = l1.LogonTime.LowPart;
                    ft.dwHighDateTime = static_cast<::DWORD>(l1.LogonTime.HighPart);
                    if (ft.dwLowDateTime != 0 || ft.dwHighDateTime != 0) {
                        const auto tp = filetime_to_time_point(ft);
                        if (!tp) {
                            return fail(tp.error());
                        }
                        session.login_time = *tp;
                    }
                }
            }
        }

        session.type = classify_windows_session_type(
            session.terminal, client_host, wts_session.SessionId);

        if (session.type == user_common::session_type::remote &&
            client_host.has_value()) {
            session.remote_host = std::move(client_host);
        }

        results.push_back(std::move(session));
    }

    std::sort(results.begin(), results.end(),
              [](const user_common::session_info& a,
                 const user_common::session_info& b) {
                  if (a.user_name != b.user_name) {
                      return a.user_name < b.user_name;
                  }
                  if (a.terminal != b.terminal) {
                      return a.terminal < b.terminal;
                  }
                  return a.session_id < b.session_id;
              });

    return results;
}

/// Returns all active user login sessions from Windows Terminal Services.
inline result<std::vector<user_common::session_info>> sessions() {
    return parse_wts_sessions_impl(
        [](::HANDLE server, ::DWORD reserved, ::DWORD version,
           ::PWTS_SESSION_INFOW* ppSessionInfo, ::DWORD* pCount) -> ::BOOL {
            return ::WTSEnumerateSessionsW(server, reserved, version,
                                           ppSessionInfo, pCount);
        },
        [](::HANDLE server, ::DWORD sessionId, ::WTS_INFO_CLASS infoClass,
           ::LPWSTR* ppBuffer, ::DWORD* pBytesReturned) -> ::BOOL {
            return ::WTSQuerySessionInformationW(server, sessionId, infoClass,
                                                 ppBuffer, pBytesReturned);
        },
        [](void* ptr) { ::WTSFreeMemory(ptr); });
}

/// Returns not_supported because Windows defines no POSIX-style numeric user
/// or group identifier concept.
///
/// Security identifiers exist but are structured platform values, so they are
/// not silently squeezed into the portable numeric contract.
inline result<std::uint32_t> real_user_id() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> effective_user_id() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> real_group_id() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> effective_group_id() {
    return fail(errc::not_supported);
}

/// Returns the login name of the user associated with the current thread.
inline result<std::string> user_name() {
    constexpr std::size_t maximum_size = 32U * 1024U;
    std::wstring value(static_cast<std::size_t>(UNLEN) + 1U, L'\0');
    for (;;) {
        ::DWORD size = static_cast<::DWORD>(value.size());
        if (::GetUserNameW(value.data(), &size) != FALSE) {
            // On success the size includes the terminating null character.
            const std::size_t length = static_cast<std::size_t>(size);
            if (length <= 1U || length > value.size()) {
                return fail(errc::malformed_data);
            }
            value.resize(length - 1U);
            break;
        }

        const ::DWORD error = ::GetLastError();
        const std::size_t required = static_cast<std::size_t>(size);
        if (error != ERROR_INSUFFICIENT_BUFFER) {
            return fail(std::error_code(static_cast<int>(error),
                                        std::system_category()));
        }
        if (required <= value.size() || required > maximum_size) {
            return fail(errc::value_too_large);
        }
        value.resize(required);
    }

    const result<std::string> converted = wide_to_utf8(value);
    if (!converted) { return converted; }
    if (converted->empty()) { return fail(errc::malformed_data); }
    return converted;
}

/// Returns the profile directory of the current user from the documented
/// known-folder interface.
inline result<std::string> home_directory() {
    ::PWSTR raw = nullptr;
    const ::HRESULT outcome =
        ::SHGetKnownFolderPath(::FOLDERID_Profile, 0U, nullptr, &raw);
    const co_task_memory guard(raw);
    if (outcome != S_OK) { return fail(map_hresult(outcome)); }

    const std::wstring value(guard.get() != nullptr ? guard.get() : L"");
    if (value.empty()) { return fail(errc::malformed_data); }
    if (!is_absolute_home_path(value)) {
        return fail(errc::malformed_data);
    }

    const result<std::string> converted = wide_to_utf8(value);
    if (!converted) { return converted; }
    if (converted->empty()) { return fail(errc::malformed_data); }
    return converted;
}

/// Returns not_supported because Windows exposes no recorded login shell
/// through an acceptable public source.
inline result<std::string> shell() {
    return fail(errc::not_supported);
}

/// Returns not_supported because Windows defines no numeric supplementary
/// group concept; group membership is expressed through security identifiers
/// that are structured platform values rather than portable numbers.
inline result<std::vector<std::uint32_t>> supplementary_groups() {
    return fail(errc::not_supported);
}

/// Owns a native token handle and closes it on every path.
class token_handle {
public:
    explicit token_handle(::HANDLE value) noexcept : value_(value) {}
    token_handle(const token_handle&) = delete;
    token_handle& operator=(const token_handle&) = delete;
    ~token_handle() {
        if (value_ != nullptr) { ::CloseHandle(value_); }
    }

    ::HANDLE get() const noexcept { return value_; }

private:
    ::HANDLE value_;
};

/// Classifies the elevation of the process token through the documented
/// token-information interface.
///
/// The query reports whether the process token holds elevated authority
/// under Windows access-control policy, which is the platform's documented
/// equivalent of privileged execution. A token that passes only filtered or
/// per-privilege grants classifies as unprivileged. Token queries preserve
/// their native error codes, including access-denied conditions.
inline result<user_common::privilege_state> classify_token_elevation(
    ::HANDLE token) {
    ::DWORD elevation = 0U;
    ::DWORD returned = 0U;
    if (!::GetTokenInformation(token, ::TokenElevation, &elevation,
                               sizeof(elevation), &returned)) {
        return fail(last_error());
    }
    if (returned != sizeof(elevation)) {
        return fail(errc::malformed_data);
    }
    using syscape::detail::user_common::privilege_state;
    return elevation != 0U ? privilege_state::privileged
                           : privilege_state::unprivileged;
}

inline result<user_common::privilege_state> privilege() {
    ::HANDLE raw = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &raw)) {
        return fail(last_error());
    }
    const token_handle guard(raw);
    return classify_token_elevation(guard.get());
}

/// Returns not_supported because Windows exposes no login-session identity
/// recorded separately from the process account through an acceptable
/// public source.
inline result<std::string> login_name() {
    return fail(errc::not_supported);
}

} // namespace user_backend
} // namespace detail
} // namespace syscape

#endif
