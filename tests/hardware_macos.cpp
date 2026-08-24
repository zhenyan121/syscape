#include <iostream>
#include <string>
#include <system_error>

#include <syscape/hardware.hpp>
#include <syscape/detail/hardware/macos.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_fact_interpretation() {
    namespace backend = syscape::detail::hardware_backend;

    const syscape::result<std::string> recorded =
        backend::interpret_text(true, std::string("Apple"));
    expect(recorded.has_value() && *recorded == "Apple",
           "A recorded identity fact passes through verbatim");

    const syscape::result<std::string> absent =
        backend::interpret_text(false, std::string("ignored"));
    expect(absent.error() == syscape::errc::not_found,
           "A platform-expert key the machine omits records absence");

    expect(backend::interpret_text(true, std::string()).error() ==
               syscape::errc::not_found,
           "An empty identity rendering records absence instead of "
           "presenting nothing as data");
}

void test_device_tree_data_conversion() {
    namespace backend = syscape::detail::hardware_backend;

    const ::UInt8 model_bytes[] = {
        'M', 'a', 'c', '1', '6', ',', '1', 0U};
    const ::CFDataRef model = ::CFDataCreate(
        ::kCFAllocatorDefault, model_bytes,
        static_cast<::CFIndex>(sizeof(model_bytes)));
    expect(model != nullptr, "A synthetic device-tree value must allocate");
    if (model != nullptr) {
        const syscape::result<std::string> converted =
            backend::copy_utf8_data(model);
        expect(converted.has_value() && *converted == "Mac16,1",
               "CFData identity text must discard its trailing terminator");
        ::CFRelease(model);
    }

    const ::UInt8 embedded_null[] = {'M', 'a', 'c', 0U, 'X', 0U};
    const ::CFDataRef malformed = ::CFDataCreate(
        ::kCFAllocatorDefault, embedded_null,
        static_cast<::CFIndex>(sizeof(embedded_null)));
    expect(malformed != nullptr, "A malformed synthetic value must allocate");
    if (malformed != nullptr) {
        expect(backend::copy_utf8_data(malformed).error() ==
                   syscape::errc::malformed_data,
               "An interior null in device-tree text must be malformed");
        ::CFRelease(malformed);
    }
}

void test_property_type_contracts() {
    namespace backend = syscape::detail::hardware_backend;

    const ::UInt8 bytes[] = {'M', 'a', 'c', 0U};
    const ::CFDataRef data = ::CFDataCreate(
        ::kCFAllocatorDefault, bytes,
        static_cast<::CFIndex>(sizeof(bytes)));
    const ::CFStringRef string = CFSTR("Mac");
    expect(data != nullptr, "A synthetic typed property must allocate");
    if (data == nullptr) { return; }

    const auto decoded_data = backend::copy_property_text(
        data, backend::property_text_kind::device_tree_data);
    expect(decoded_data.has_value() && *decoded_data == "Mac",
           "A device-tree text property must accept CFData");
    expect(backend::copy_property_text(
               string, backend::property_text_kind::device_tree_data)
               .error() == syscape::errc::malformed_data,
           "A device-tree text property must reject CFString");

    const auto decoded_string = backend::copy_property_text(
        string, backend::property_text_kind::core_foundation_string);
    expect(decoded_string.has_value() && *decoded_string == "Mac",
           "A platform UUID text property must accept CFString");
    expect(backend::copy_property_text(
               data, backend::property_text_kind::core_foundation_string)
               .error() == syscape::errc::malformed_data,
           "A platform UUID text property must reject CFData");
    ::CFRelease(data);
}

/// Exercises every public query against the running system. The checks stay
/// tolerant about which facts this particular machine records, because the
/// interface documents per-key absence, but they require every outcome to be
/// explicit rather than fabricated.
template <typename Query>
void check_live(const char* label, const Query& query) {
    const auto value = query();
    if (value) { return; }
    const std::error_code error = value.error();
    if (error == syscape::errc::not_supported ||
        error == syscape::errc::not_found) {
        return;
    }
    std::cerr << "FAIL: " << label << " reported " << error.message()
              << '\n';
    ++failures;
}

} // namespace

int main() {
    test_fact_interpretation();
    test_device_tree_data_conversion();
    test_property_type_contracts();

    check_live("system_manufacturer", syscape::hardware::system_manufacturer);
    check_live("system_product_name", syscape::hardware::system_product_name);
    check_live("system_product_version",
               syscape::hardware::system_product_version);
    check_live("motherboard_manufacturer",
               syscape::hardware::motherboard_manufacturer);
    check_live("motherboard_product_name",
               syscape::hardware::motherboard_product_name);
    check_live("motherboard_version", syscape::hardware::motherboard_version);
    check_live("firmware_vendor", syscape::hardware::firmware_vendor);
    check_live("firmware_version", syscape::hardware::firmware_version);
    check_live("firmware_release_date",
               syscape::hardware::firmware_release_date);
    check_live("chassis_form_factor",
               syscape::hardware::chassis_form_factor);
    check_live("hardware_uuid", syscape::hardware::hardware_uuid);

    return failures == 0 ? 0 : 1;
}
