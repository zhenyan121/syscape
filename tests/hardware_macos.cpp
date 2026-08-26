#include <cstdint>
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

void test_inventory_property_conversion() {
    namespace backend = syscape::detail::hardware_backend;

    const ::UInt8 little_endian[] = {0x86U, 0x80U, 0x00U, 0x00U};
    const ::CFDataRef data = ::CFDataCreate(
        ::kCFAllocatorDefault, little_endian,
        static_cast<::CFIndex>(sizeof(little_endian)));
    expect(data != nullptr, "A synthetic PCI property must allocate");
    if (data != nullptr) {
        const auto value = backend::optional_cfdata_u32(data);
        expect(value.has_value() && value->has_value() && **value == 0x8086U,
               "PCI CFData numbers must decode explicitly as little endian");
        ::CFRelease(data);
    }

    const ::UInt8 assigned_addresses[20] = {
        0x00U, 0x08U, 0x02U, 0x00U,
        0U, 0U, 0U, 0U, 0U, 0U,
        0U, 0U, 0U, 0U, 0U, 0U,
        0U, 0U, 0U, 0U};
    const ::CFDataRef address_data = ::CFDataCreate(
        ::kCFAllocatorDefault, assigned_addresses,
        static_cast<::CFIndex>(sizeof(assigned_addresses)));
    expect(address_data != nullptr,
           "A synthetic assigned-addresses array must allocate");
    if (address_data != nullptr) {
        const auto first =
            backend::optional_assigned_address_phys_hi(address_data);
        expect(first.has_value() && first->has_value() &&
                   **first == 0x00020800U,
               "A multi-cell assigned-addresses value must expose phys.hi");
        expect(backend::optional_cfdata_u32(address_data).error() ==
                   syscape::errc::malformed_data,
               "Scalar PCI properties must still reject extra cells");
        ::CFRelease(address_data);
    }

    const ::UInt8 truncated_addresses[5] = {0U, 0U, 0U, 0U, 0U};
    const ::CFDataRef truncated_data = ::CFDataCreate(
        ::kCFAllocatorDefault, truncated_addresses,
        static_cast<::CFIndex>(sizeof(truncated_addresses)));
    expect(truncated_data != nullptr,
           "A synthetic truncated assigned-addresses array must allocate");
    if (truncated_data != nullptr) {
        expect(backend::optional_assigned_address_phys_hi(truncated_data)
                   .error() == syscape::errc::malformed_data,
               "An incomplete assigned-addresses entry must fail");
        ::CFRelease(truncated_data);
    }

    const ::CFDataRef empty_addresses = ::CFDataCreate(
        ::kCFAllocatorDefault, nullptr, 0);
    expect(empty_addresses != nullptr,
           "An empty assigned-addresses array must allocate");
    if (empty_addresses != nullptr) {
        const auto empty =
            backend::optional_assigned_address_phys_hi(empty_addresses);
        expect(empty.has_value() && !empty->has_value(),
               "An empty assigned-addresses array must leave BDF unknown");
        ::CFRelease(empty_addresses);
    }

    std::int64_t number_value = 0x1234;
    const ::CFNumberRef number = ::CFNumberCreate(
        ::kCFAllocatorDefault, ::kCFNumberSInt64Type, &number_value);
    expect(number != nullptr, "A synthetic USB number must allocate");
    if (number != nullptr) {
        const auto value = backend::optional_cfnumber_u64(number);
        expect(value.has_value() && value->has_value() && **value == 0x1234U,
               "USB CFNumber values must preserve their unsigned magnitude");
        expect(backend::optional_cfdata_u32(number).error() ==
                   syscape::errc::malformed_data,
               "A PCI data property with the wrong CF type must fail");
        ::CFRelease(number);
    }
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
    test_inventory_property_conversion();

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
    check_live("pci_devices", syscape::hardware::pci_devices);
    check_live("usb_devices", syscape::hardware::usb_devices);
    check_live("memory_devices", syscape::hardware::memory_devices);

    return failures == 0 ? 0 : 1;
}
