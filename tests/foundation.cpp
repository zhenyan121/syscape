#include <climits>
#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>

#include <syscape/architecture.hpp>
#include <syscape/capability.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/error.hpp>
#include <syscape/execution_environment.hpp>
#include <syscape/result.hpp>
#include <syscape/toolchain.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_architecture() {
    const syscape::data_model_info model = syscape::target_data_model_info();
    expect(model.short_bits == sizeof(short) * CHAR_BIT,
           "short width must match sizeof");
    expect(model.int_bits == sizeof(int) * CHAR_BIT,
           "int width must match sizeof");
    expect(model.long_bits == sizeof(long) * CHAR_BIT,
           "long width must match sizeof");
    expect(model.long_long_bits == sizeof(long long) * CHAR_BIT,
           "long long width must match sizeof");
    expect(model.pointer_bits == sizeof(void*) * CHAR_BIT,
           "pointer width must match sizeof");
    expect(syscape::architecture_name(syscape::target_architecture()) != nullptr,
           "architecture name must exist");
    expect(syscape::byte_order_name(syscape::target_byte_order()) != nullptr,
           "byte-order name must exist");
    expect(syscape::data_model_name(syscape::target_data_model()) != nullptr,
           "data-model name must exist");
}

void test_toolchain() {
    expect(syscape::target_cpp_version() >= 201703L,
           "toolchain must report C++17 or later");
    expect(syscape::compiler_name(syscape::target_compiler()) != nullptr,
           "compiler name must exist");
    expect(syscape::standard_library_name(syscape::target_standard_library()) !=
               nullptr,
           "standard-library name must exist");
    const syscape::toolchain_version version =
        syscape::target_compiler_version();
    expect(version.major > 0U, "known test compiler must have a major version");
}

void test_environment() {
    expect(syscape::operating_system_name(syscape::target_operating_system()) !=
               nullptr,
           "operating-system name must exist");
    expect(syscape::execution_environment_name(
               syscape::target_execution_environment()) != nullptr,
           "execution-environment name must exist");
}

void test_capability() {
    const syscape::capability unknown;
    const syscape::capability available(syscape::capability_state::available);
    expect(!unknown.recognized(), "default capability must be unknown");
    expect(!unknown.available(), "unknown capability must not be available");
    expect(available.recognized(), "available capability must be recognized");
    expect(static_cast<bool>(available), "available capability must convert to true");
}

void test_errors() {
    const std::error_code unsupported = syscape::errc::not_supported;
    expect(unsupported.category() == syscape::error_category(),
           "portable errors must use the Syscape category");
    expect(unsupported == std::errc::operation_not_supported,
           "not_supported must map to the standard condition");
    expect(syscape::make_error_code(syscape::errc::success).value() == 0,
           "success must use value zero");
}

void test_results() {
    syscape::result<std::string> value(std::string("syscape"));
    expect(value.has_value(), "value result must report success");
    expect(value->size() == 7U, "operator arrow must access the value");
    expect(*value == "syscape", "operator star must access the value");
    expect(value.error().value() == 0, "successful result must expose success error");

    syscape::result<std::string> failure =
        syscape::fail(syscape::errc::permission_denied);
    expect(!failure, "failed result must convert to false");
    expect(failure.error() == std::errc::permission_denied,
           "failed result must preserve its error");
    expect(failure.value_or("fallback") == "fallback",
           "value_or must return its fallback on failure");

    bool threw = false;
    try {
        static_cast<void>(failure.value());
    } catch (const syscape::bad_result_access& error) {
        threw = error.error() == std::errc::permission_denied;
    }
    expect(threw, "value must throw bad_result_access on failure");

    const syscape::result<void> void_success;
    const syscape::result<void> void_failure =
        syscape::fail(syscape::errc::not_supported);
    expect(void_success.has_value(), "default void result must succeed");
    expect(!void_failure, "failed void result must convert to false");
}

void test_utf() {
    const std::string utf8 = u8"Syscape: 世界 😀";
    const std::u16string utf16 = u"Syscape: 世界 😀";

    expect(syscape::detail::is_valid_utf8(utf8), "valid UTF-8 must be accepted");
    const syscape::result<std::u16string> converted16 =
        syscape::detail::utf8_to_utf16(utf8);
    expect(converted16 && *converted16 == utf16,
           "UTF-8 must convert to the expected UTF-16");
    const syscape::result<std::string> converted8 =
        syscape::detail::utf16_to_utf8(utf16);
    expect(converted8 && *converted8 == utf8,
           "UTF-16 must convert to the expected UTF-8");

    const std::string overlong("\xC0\xAF", 2U);
    const std::string truncated("\xF0\x9F\x98", 3U);
    expect(!syscape::detail::is_valid_utf8(overlong),
           "overlong UTF-8 must be rejected");
    expect(!syscape::detail::is_valid_utf8(truncated),
           "truncated UTF-8 must be rejected");
    expect(!syscape::detail::utf8_to_utf16(overlong),
           "invalid UTF-8 conversion must fail");

    const std::u16string lone_high(1U, static_cast<char16_t>(0xD800U));
    const std::u16string lone_low(1U, static_cast<char16_t>(0xDC00U));
    expect(!syscape::detail::utf16_to_utf8(lone_high),
           "unpaired high surrogate must be rejected");
    expect(!syscape::detail::utf16_to_utf8(lone_low),
           "unpaired low surrogate must be rejected");
}

} // namespace

int main() {
    test_architecture();
    test_toolchain();
    test_environment();
    test_capability();
    test_errors();
    test_results();
    test_utf();
    return failures == 0 ? 0 : 1;
}
