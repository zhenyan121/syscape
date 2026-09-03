#include <cmath>
#include <iostream>
#include <string>
#include <type_traits>

#include <syscape/detail/openharmony/parameter.hpp>
#include <syscape/detail/os/openharmony.hpp>
#include <syscape/os.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void expect_nonempty_string(const syscape::result<std::string>& value,
                            const char* message) {
    expect(value && !value->empty(), message);
}

void test_uptime_parsing() {
    using syscape::detail::os_backend::parse_proc_uptime;
    const auto valid = parse_proc_uptime("1234.56 7890.12\n");
    expect(valid && valid->count() == 1234560,
           "valid uptime must convert to 1234560 ms");

    const auto whole_sec = parse_proc_uptime("42 100\n");
    expect(whole_sec && whole_sec->count() == 42000,
           "integer seconds must convert to 42000 ms");

    const auto junk = parse_proc_uptime("123junk 456\n");
    expect(!junk && junk.error() == syscape::errc::malformed_data,
           "trailing text in seconds token must be malformed_data");

    const auto negative = parse_proc_uptime("-10.5 20.0\n");
    expect(!negative && negative.error() == syscape::errc::malformed_data,
           "negative uptime must be malformed_data");

    const auto empty = parse_proc_uptime("   \n");
    expect(!empty && empty.error() == syscape::errc::malformed_data,
           "empty uptime content must be malformed_data");

    const auto overflow = parse_proc_uptime("1e300 0.0\n");
    expect(!overflow && overflow.error() == syscape::errc::value_too_large,
           "huge uptime value must report value_too_large");
}

void test_parameter_signatures_and_errors() {
#if defined(__OHOS__) || defined(__OpenHarmony__)
    static_assert(
        std::is_same<
            syscape::detail::openharmony::beget_detail::SystemReadParamFn,
            int (*)(const char*, char*, unsigned int*)>::value,
        "SystemReadParamFn signature must take 3 args");
    static_assert(
        std::is_same<syscape::detail::openharmony::beget_detail::GetParameterFn,
                     int (*)(const char*, const char*, char*,
                             unsigned int)>::value,
        "GetParameterFn signature must take 4 args with len by value");

    using syscape::detail::openharmony::beget_detail::map_param_error;
    expect(map_param_error(116) == syscape::errc::permission_denied,
           "PARAM_CODE_PERMISSION_DENIED must map to permission_denied");
    expect(map_param_error(1001) == syscape::errc::permission_denied,
           "DAC_RESULT_FORBIDED must map to permission_denied");
    expect(map_param_error(1002) == syscape::errc::permission_denied,
           "SELINUX_RESULT_FORBIDED must map to permission_denied");
    expect(map_param_error(-14700103) == syscape::errc::permission_denied,
           "SYSPARAM_PERMISSION_DENIED must map to permission_denied");

    expect(map_param_error(100) == syscape::errc::invalid_argument,
           "PARAM_CODE_INVALID_PARAM must map to invalid_argument");
    expect(map_param_error(-9) == syscape::errc::invalid_argument,
           "EC_INVALID must map to invalid_argument");
    expect(map_param_error(-401) == syscape::errc::invalid_argument,
           "SYSPARAM_INVALID_INPUT must map to invalid_argument");
    expect(map_param_error(-14700102) == syscape::errc::invalid_argument,
           "SYSPARAM_INVALID_VALUE must map to invalid_argument");
    expect(map_param_error(109) == syscape::errc::invalid_argument,
           "PARAM_CODE_NODE_EXIST must map to invalid_argument");
    expect(map_param_error(110) == syscape::errc::invalid_argument,
           "PARAM_WATCHER_CALLBACK_EXIST must map to invalid_argument");

    expect(map_param_error(104) == syscape::errc::not_supported,
           "PARAM_CODE_NOT_SUPPORT must map to not_supported");
    expect(map_param_error(105) == syscape::errc::temporarily_unavailable,
           "PARAM_CODE_TIMEOUT must map to temporarily_unavailable");
    expect(map_param_error(108) == syscape::errc::temporarily_unavailable,
           "PARAM_CODE_IPC_ERROR must map to temporarily_unavailable");
    expect(map_param_error(111) == syscape::errc::temporarily_unavailable,
           "PARAM_WATCHER_GET_SERVICE_FAILED must map to "
           "temporarily_unavailable");
    expect(map_param_error(113) == syscape::errc::temporarily_unavailable,
           "PARAM_WORKSPACE_NOT_INIT must map to temporarily_unavailable");
    expect(map_param_error(114) == syscape::errc::temporarily_unavailable,
           "PARAM_CODE_FAIL_CONNECT must map to temporarily_unavailable");
    expect(map_param_error(-14700105) == syscape::errc::temporarily_unavailable,
           "SYSPARAM_WAIT_TIMEOUT must map to temporarily_unavailable");

    expect(map_param_error(115) == syscape::errc::resource_exhausted,
           "PARAM_CODE_MEMORY_NOT_ENOUGH must map to resource_exhausted");
    expect(map_param_error(112) == syscape::errc::resource_exhausted,
           "PARAM_CODE_MEMORY_MAP_FAILED must map to resource_exhausted");
    expect(map_param_error(117) == syscape::errc::resource_exhausted,
           "PARAM_DEFAULT_PARAM_MEMORY_NOT_ENOUGH must map to "
           "resource_exhausted");

    expect(map_param_error(106) == syscape::errc::not_found,
           "PARAM_CODE_NOT_FOUND must map to not_found");
    expect(map_param_error(-14700101) == syscape::errc::not_found,
           "SYSPARAM_NOT_FOUND must map to not_found");
    expect(map_param_error(-1) == syscape::errc::io_error,
           "PARAM_CODE_ERROR must map to io_error");

    const auto invalid_null =
        syscape::detail::openharmony::get_parameter(nullptr);
    expect(!invalid_null &&
               invalid_null.error() == syscape::errc::invalid_argument,
           "get_parameter with nullptr key must fail with invalid_argument");
    const auto invalid_empty = syscape::detail::openharmony::get_parameter("");
    expect(!invalid_empty &&
               invalid_empty.error() == syscape::errc::invalid_argument,
           "get_parameter with empty key must fail with invalid_argument");
#endif
}

void test_runtime_queries() {
    const auto product = syscape::os::product_name();
    expect((product && !product->empty()) ||
               product.error() == syscape::errc::not_found,
           "product name must be nonempty or report not_found");
    expect_nonempty_string(syscape::os::kernel_name(),
                           "kernel name must be nonempty");
    expect_nonempty_string(syscape::os::kernel_version(),
                           "kernel version must be nonempty");
    const auto host = syscape::os::host_name();
    expect((host && !host->empty()) || host.error() == syscape::errc::not_found,
           "host name must be nonempty or report not_found");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed && elapsed->count() >= 0,
           "uptime must be a nonnegative duration");

    const auto started = syscape::os::boot_time();
    expect(started.has_value(), "boot time query must succeed");

    const auto version = syscape::os::product_version();
    expect(version.has_value() || version.error() == syscape::errc::not_found ||
               version.error() == syscape::errc::not_supported,
           "product_version must succeed or report expected error");

    const auto build_id = syscape::os::build_identifier();
    expect(build_id.has_value() ||
               build_id.error() == syscape::errc::not_found ||
               build_id.error() == syscape::errc::not_supported,
           "build_identifier must succeed or report expected error");
}

} // namespace

int main() {
    test_uptime_parsing();
    test_parameter_signatures_and_errors();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
