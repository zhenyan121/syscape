#ifndef SYSCAPE_DETAIL_OS_WINDOWS_HPP
#define SYSCAPE_DETAIL_OS_WINDOWS_HPP

#include <chrono>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <windows.h>
#include <winternl.h>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

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

inline result<RTL_OSVERSIONINFOW> version_information() {
    const HMODULE module = ::GetModuleHandleW(L"ntdll.dll");
    if (module == nullptr) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    const FARPROC address = ::GetProcAddress(module, "RtlGetVersion");
    if (address == nullptr) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    using function_type = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    const auto function = reinterpret_cast<function_type>(address);
    RTL_OSVERSIONINFOW value {};
    value.dwOSVersionInfoSize = sizeof(value);
    if (function(&value) < 0) { return fail(errc::io_error); }
    return value;
}

inline result<std::string> product_name() { return std::string("Windows"); }

inline result<std::string> product_version() {
    const result<RTL_OSVERSIONINFOW> value = version_information();
    if (!value) { return fail(value.error()); }
    return std::to_string(value->dwMajorVersion) + "." +
           std::to_string(value->dwMinorVersion);
}

inline result<std::string> build_identifier() {
    const result<RTL_OSVERSIONINFOW> value = version_information();
    return value ? result<std::string>(std::to_string(value->dwBuildNumber))
                 : result<std::string>(fail(value.error()));
}

inline result<std::string> kernel_name() { return std::string("Windows NT"); }

inline result<std::string> kernel_version() {
    const result<RTL_OSVERSIONINFOW> value = version_information();
    if (!value) { return fail(value.error()); }
    return std::to_string(value->dwMajorVersion) + "." +
           std::to_string(value->dwMinorVersion) + "." +
           std::to_string(value->dwBuildNumber);
}

inline result<std::string> host_name() {
    DWORD size = 0U;
    static_cast<void>(::GetComputerNameExW(ComputerNameDnsHostname, nullptr, &size));
    if (size == 0U) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    std::wstring value(size, L'\0');
    if (!::GetComputerNameExW(ComputerNameDnsHostname, &value[0], &size)) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    value.resize(size);
    return wide_to_utf8(value);
}

inline result<std::chrono::milliseconds> uptime() {
    const ULONGLONG value = ::GetTickCount64();
    using representation = std::chrono::milliseconds::rep;
    if (value > static_cast<ULONGLONG>(
                    (std::numeric_limits<representation>::max)())) {
        return fail(errc::value_too_large);
    }
    return std::chrono::milliseconds(static_cast<representation>(value));
}

inline result<std::chrono::system_clock::time_point> boot_time() {
    const result<std::chrono::milliseconds> elapsed = uptime();
    return elapsed ? result<std::chrono::system_clock::time_point>(
                         std::chrono::system_clock::now() - *elapsed)
                   : result<std::chrono::system_clock::time_point>(
                         fail(elapsed.error()));
}

inline result<std::string> boot_identifier() { return fail(errc::not_supported); }

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
