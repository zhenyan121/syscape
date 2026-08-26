#ifndef SYSCAPE_DETAIL_HARDWARE_MACOS_HPP
#define SYSCAPE_DETAIL_HARDWARE_MACOS_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#include <syscape/detail/hardware/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace hardware_backend {

/// The documented registry class describing the machine's platform expert,
/// whose properties carry the identity facts this module exposes.
constexpr const char* platform_expert_class = "IOPlatformExpertDevice";

/// Owns one CoreFoundation object reference for the duration of a query.
class cf_object {
public:
    explicit cf_object(::CFTypeRef value) noexcept : value_(value) {}
    cf_object(const cf_object&) = delete;
    cf_object& operator=(const cf_object&) = delete;
    ~cf_object() {
        if (value_ != nullptr) { ::CFRelease(value_); }
    }

    /// Returns the owned reference.
    ::CFTypeRef get() const noexcept { return value_; }

private:
    ::CFTypeRef value_;
};

/// Owns one IOKit object reference obtained from a matching call.
class io_object_guard {
public:
    explicit io_object_guard(::io_service_t value) noexcept : value_(value) {}
    io_object_guard(const io_object_guard&) = delete;
    io_object_guard& operator=(const io_object_guard&) = delete;
    ~io_object_guard() {
        if (value_ != IO_OBJECT_NULL) { ::IOObjectRelease(value_); }
    }

private:
    ::io_service_t value_;
};

/// Copies one CoreFoundation string into UTF-8 storage.
inline result<std::string> copy_utf8_string(::CFStringRef value) {
    if (value == nullptr) { return fail(errc::io_error); }
    const ::CFIndex length = ::CFStringGetLength(value);
    const ::CFIndex maximum = ::CFStringGetMaximumSizeForEncoding(
        length, ::kCFStringEncodingUTF8);
    if (maximum < 0) { return fail(errc::io_error); }
    std::string output;
    output.resize(static_cast<std::size_t>(maximum) + 1U);
    if (!::CFStringGetCString(value, &output[0],
                              static_cast<::CFIndex>(output.size()),
                              ::kCFStringEncodingUTF8)) {
        return fail(errc::invalid_encoding);
    }
    output.resize(std::char_traits<char>::length(output.c_str()));
    return output;
}

/// Copies one device-tree byte-string property into UTF-8 candidate storage.
///
/// IOKit exposes Open Firmware device-tree strings such as manufacturer,
/// model, and board-id as CFData rather than CFString. Their byte payload can
/// carry one trailing C terminator, which is representation rather than part
/// of the public value. An interior null would truncate the recorded identity
/// in C-oriented consumers and therefore reports malformed platform data.
inline result<std::string> copy_utf8_data(::CFDataRef value) {
    if (value == nullptr) { return fail(errc::io_error); }
    const ::CFIndex recorded_length = ::CFDataGetLength(value);
    if (recorded_length < 0) { return fail(errc::malformed_data); }
    std::size_t length = static_cast<std::size_t>(recorded_length);
    const ::UInt8* bytes = ::CFDataGetBytePtr(value);
    if (length != 0U && bytes == nullptr) { return fail(errc::io_error); }
    if (length != 0U && bytes[length - 1U] == 0U) { --length; }
    for (std::size_t index = 0U; index < length; ++index) {
        if (bytes[index] == 0U) { return fail(errc::malformed_data); }
    }
    if (length == 0U) { return std::string(); }
    return std::string(reinterpret_cast<const char*>(bytes), length);
}

/// The plain identity facts recorded by the platform-expert properties.
///
/// Keys the platform does not record leave the corresponding flags false so
/// interpretation can distinguish an unrecorded fact from a recorded one.
struct platform_facts {
    /// The documented manufacturer property.
    bool has_manufacturer = false;
    std::string manufacturer;
    /// The documented model property, which names the platform's own model
    /// identifier vocabulary.
    bool has_product_name = false;
    std::string product_name;
    /// The documented board-id property, which names the board within the
    /// platform's own vocabulary.
    bool has_board_product = false;
    std::string board_product;
    /// The documented IOPlatformUUID property.
    bool has_uuid = false;
    std::string uuid;
};

/// The CoreFoundation representation documented for one registry property.
enum class property_text_kind {
    device_tree_data,
    core_foundation_string
};

/// Copies one already-owned registry property using its documented type.
inline result<std::string> copy_property_text(
    ::CFTypeRef value, property_text_kind expected_kind) {
    if (value == nullptr) { return fail(errc::io_error); }
    const ::CFTypeID type = ::CFGetTypeID(value);
    if (expected_kind == property_text_kind::device_tree_data) {
        if (type != ::CFDataGetTypeID()) { return fail(errc::malformed_data); }
        return copy_utf8_data(static_cast<::CFDataRef>(value));
    }
    if (type != ::CFStringGetTypeID()) { return fail(errc::malformed_data); }
    return copy_utf8_string(static_cast<::CFStringRef>(value));
}

/// Copies one recorded registry text property into plain storage.
///
/// An absent key records an absent field because platforms legitimately omit
/// individual identity properties. Platform-expert UUID properties are
/// CFString values, while device-tree identity strings commonly arrive as
/// CFData byte strings. A present value of any other type contradicts these
/// contracts and fails as malformed platform data.
inline result<void> read_property(::io_service_t service, ::CFStringRef key,
                                  property_text_kind expected_kind,
                                  bool& present, std::string& destination) {
    const cf_object reference(::IORegistryEntryCreateCFProperty(
        service, key, ::kCFAllocatorDefault, 0));
    if (reference.get() == nullptr) { return {}; }
    const result<std::string> text =
        copy_property_text(reference.get(), expected_kind);
    if (!text) { return fail(text.error()); }
    present = true;
    destination = std::move(*text);
    return {};
}

/// Extracts every recorded identity property of one platform-expert entry.
inline result<platform_facts> extract_platform_facts(
    ::io_service_t service) {
    platform_facts facts;
    const result<void> stored_manufacturer =
        read_property(service, CFSTR("manufacturer"),
                      property_text_kind::device_tree_data,
                      facts.has_manufacturer, facts.manufacturer);
    if (!stored_manufacturer) { return fail(stored_manufacturer.error()); }
    const result<void> stored_model =
        read_property(service, CFSTR("model"),
                      property_text_kind::device_tree_data,
                      facts.has_product_name,
                      facts.product_name);
    if (!stored_model) { return fail(stored_model.error()); }
    const result<void> stored_board =
        read_property(service, CFSTR("board-id"),
                      property_text_kind::device_tree_data,
                      facts.has_board_product,
                      facts.board_product);
    if (!stored_board) { return fail(stored_board.error()); }
    const result<void> stored_uuid =
        read_property(service, CFSTR("IOPlatformUUID"),
                      property_text_kind::core_foundation_string,
                      facts.has_uuid,
                      facts.uuid);
    if (!stored_uuid) { return fail(stored_uuid.error()); }
    return facts;
}

/// Locates the platform-expert entry and extracts its identity facts.
///
/// Newer SDKs name the default master port kIOMainPortDefault while older
/// SDKs only declare kIOMasterPortDefault; both constants carry the same
/// value. The matching call consumes the dictionary reference the matcher
/// creates, so no separate release applies to it.
inline result<platform_facts> collect_platform_facts() {
#if defined(kIOMainPortDefault)
    const ::io_service_t port = kIOMainPortDefault;
#else
    const ::io_service_t port = kIOMasterPortDefault;
#endif
    ::CFMutableDictionaryRef matching =
        ::IOServiceMatching(platform_expert_class);
    if (matching == nullptr) { return fail(errc::io_error); }
    const ::io_service_t service =
        ::IOServiceGetMatchingService(port, matching);
    if (service == IO_OBJECT_NULL) { return fail(errc::not_found); }
    const io_object_guard owned(service);
    static_cast<void>(owned);
    return extract_platform_facts(service);
}

/// Reduces one optional text fact into a query answer.
///
/// A property whose recorded rendering decodes to nothing contributes no
/// usable identity, so it records absence exactly like an omitted key.
inline result<std::string> interpret_text(bool present,
                                          const std::string& value) {
    if (!present || value.empty()) { return fail(errc::not_found); }
    return value;
}

inline result<std::string> system_manufacturer() {
    const result<platform_facts> facts = collect_platform_facts();
    if (!facts) { return fail(facts.error()); }
    return interpret_text(facts->has_manufacturer, facts->manufacturer);
}

inline result<std::string> system_product_name() {
    const result<platform_facts> facts = collect_platform_facts();
    if (!facts) { return fail(facts.error()); }
    return interpret_text(facts->has_product_name, facts->product_name);
}

inline result<std::string> system_product_version() {
    // Darwin records no product-version property on the platform expert, so
    // the query reports absence instead of deriving a value from unrelated
    // model renderings.
    return fail(errc::not_found);
}

inline result<std::string> motherboard_manufacturer() {
    // Darwin records the board identifier but no separate board-vendor
    // property, so this query reports absence rather than copying the system
    // manufacturer under a second name.
    return fail(errc::not_found);
}

inline result<std::string> motherboard_product_name() {
    const result<platform_facts> facts = collect_platform_facts();
    if (!facts) { return fail(facts.error()); }
    return interpret_text(facts->has_board_product, facts->board_product);
}

inline result<std::string> motherboard_version() {
    // Darwin records no board-version property on the platform expert.
    return fail(errc::not_found);
}

inline result<std::string> firmware_vendor() {
    // Darwin exposes no publicly documented firmware-vendor source through
    // the platform-expert registry that holds across architectures, so the
    // query reports the capability as unsupported instead of guessing.
    return fail(errc::not_supported);
}

inline result<std::string> firmware_version() {
    // See firmware_vendor(): no publicly documented source exists.
    return fail(errc::not_supported);
}

inline result<std::string> firmware_release_date() {
    // See firmware_vendor(): no publicly documented source exists.
    return fail(errc::not_supported);
}

inline result<hardware_common::chassis_classification> chassis_form_factor() {
    // Darwin exposes no chassis-classification record through a public
    // interface, so the query reports the capability as unsupported.
    return fail(errc::not_supported);
}

inline result<std::string> hardware_uuid() {
    const result<platform_facts> facts = collect_platform_facts();
    if (!facts) { return fail(facts.error()); }
    return interpret_text(facts->has_uuid, facts->uuid);
}

inline result<std::uint32_t> copy_cfdata_u32(::CFDataRef value) {
    if (value == nullptr) { return fail(errc::io_error); }
    const ::CFIndex len = ::CFDataGetLength(value);
    if (len != 4) { return fail(errc::malformed_data); }
    const ::UInt8* bytes = ::CFDataGetBytePtr(value);
    if (bytes == nullptr) { return fail(errc::io_error); }
    return hardware_common::read_le_u32(bytes);
}

/// Reads the first 32-bit cell of a nonempty Open Firmware data array.
inline result<std::uint32_t> copy_cfdata_first_u32(::CFDataRef value) {
    if (value == nullptr) { return fail(errc::io_error); }
    const ::CFIndex len = ::CFDataGetLength(value);
    if (len < 4) { return fail(errc::malformed_data); }
    const ::UInt8* bytes = ::CFDataGetBytePtr(value);
    if (bytes == nullptr) { return fail(errc::io_error); }
    return hardware_common::read_le_u32(bytes);
}

inline result<std::uint64_t> copy_cfnumber_u64(::CFNumberRef value) {
    if (value == nullptr) { return fail(errc::io_error); }
    std::int64_t num = 0;
    if (!::CFNumberGetValue(value, kCFNumberSInt64Type, &num) || num < 0) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint64_t>(num);
}

inline result<std::optional<std::uint32_t>> optional_cfdata_u32(
    ::CFTypeRef value) {
    if (value == nullptr) { return std::optional<std::uint32_t>(); }
    if (::CFGetTypeID(value) != ::CFDataGetTypeID()) {
        return fail(errc::malformed_data);
    }
    const result<std::uint32_t> number =
        copy_cfdata_u32(static_cast<::CFDataRef>(value));
    if (!number) { return fail(number.error()); }
    return std::optional<std::uint32_t>(*number);
}

/// Reads phys.hi from an Open Firmware PCI assigned-addresses array. Every
/// nonempty entry contains three address cells and two size cells.
inline result<std::optional<std::uint32_t>> optional_assigned_address_phys_hi(
    ::CFTypeRef value) {
    if (value == nullptr) { return std::optional<std::uint32_t>(); }
    if (::CFGetTypeID(value) != ::CFDataGetTypeID()) {
        return fail(errc::malformed_data);
    }
    const ::CFDataRef data = static_cast<::CFDataRef>(value);
    const ::CFIndex length = ::CFDataGetLength(data);
    constexpr ::CFIndex entry_size = 5 * 4;
    if (length == 0) { return std::optional<std::uint32_t>(); }
    if (length < 0 || length % entry_size != 0) {
        return fail(errc::malformed_data);
    }
    const result<std::uint32_t> number = copy_cfdata_first_u32(data);
    if (!number) { return fail(number.error()); }
    return std::optional<std::uint32_t>(*number);
}

inline result<std::optional<std::uint64_t>> optional_cfnumber_u64(
    ::CFTypeRef value) {
    if (value == nullptr) { return std::optional<std::uint64_t>(); }
    if (::CFGetTypeID(value) != ::CFNumberGetTypeID()) {
        return fail(errc::malformed_data);
    }
    const result<std::uint64_t> number =
        copy_cfnumber_u64(static_cast<::CFNumberRef>(value));
    if (!number) { return fail(number.error()); }
    return std::optional<std::uint64_t>(*number);
}

inline result<std::optional<std::string>> optional_cfstring_utf8(
    ::CFTypeRef value) {
    if (value == nullptr) { return std::optional<std::string>(); }
    if (::CFGetTypeID(value) != ::CFStringGetTypeID()) {
        return fail(errc::malformed_data);
    }
    const result<std::string> text =
        copy_utf8_string(static_cast<::CFStringRef>(value));
    if (!text) { return fail(text.error()); }
    if (text->empty()) { return std::optional<std::string>(); }
    return std::optional<std::string>(*text);
}

inline result<std::vector<::syscape::hardware::pci_device>> pci_devices() {
#if defined(kIOMainPortDefault)
    const ::io_service_t port = kIOMainPortDefault;
#else
    const ::io_service_t port = kIOMasterPortDefault;
#endif
    ::CFMutableDictionaryRef matching = ::IOServiceMatching("IOPCIDevice");
    if (matching == nullptr) { return fail(errc::io_error); }
    ::io_iterator_t iterator = IO_OBJECT_NULL;
    if (::IOServiceGetMatchingServices(port, matching, &iterator) != KERN_SUCCESS ||
        iterator == IO_OBJECT_NULL) {
        return fail(errc::io_error);
    }
    const io_object_guard iter_guard(iterator);

    std::vector<::syscape::hardware::pci_device> result_devices;
    ::io_service_t service = IO_OBJECT_NULL;
    while ((service = ::IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        const io_object_guard service_guard(service);
        ::syscape::hardware::pci_device dev;

        // vendor-id
        const cf_object ven_ref(::IORegistryEntryCreateCFProperty(
            service, CFSTR("vendor-id"), ::kCFAllocatorDefault, 0));
        const auto ven_val = optional_cfdata_u32(ven_ref.get());
        if (!ven_val) { return fail(ven_val.error()); }
        if (!*ven_val || **ven_val > 0xFFFFU) { return fail(errc::malformed_data); }
        dev.vendor_id = static_cast<std::uint16_t>(**ven_val);

        // device-id
        const cf_object dev_ref(::IORegistryEntryCreateCFProperty(
            service, CFSTR("device-id"), ::kCFAllocatorDefault, 0));
        const auto device_val = optional_cfdata_u32(dev_ref.get());
        if (!device_val) { return fail(device_val.error()); }
        if (!*device_val || **device_val > 0xFFFFU) { return fail(errc::malformed_data); }
        dev.device_id = static_cast<std::uint16_t>(**device_val);

        // subsystem-vendor-id
        const cf_object sven_ref(::IORegistryEntryCreateCFProperty(
            service, CFSTR("subsystem-vendor-id"), ::kCFAllocatorDefault, 0));
        const auto sven_val = optional_cfdata_u32(sven_ref.get());
        if (!sven_val) { return fail(sven_val.error()); }
        if (*sven_val) {
            if (**sven_val > 0xFFFFU) { return fail(errc::malformed_data); }
            dev.subsystem_vendor_id = static_cast<std::uint16_t>(**sven_val);
        }

        // subsystem-id
        const cf_object sdev_ref(::IORegistryEntryCreateCFProperty(
            service, CFSTR("subsystem-id"), ::kCFAllocatorDefault, 0));
        const auto sdev_val = optional_cfdata_u32(sdev_ref.get());
        if (!sdev_val) { return fail(sdev_val.error()); }
        if (*sdev_val) {
            if (**sdev_val > 0xFFFFU) { return fail(errc::malformed_data); }
            dev.subsystem_device_id = static_cast<std::uint16_t>(**sdev_val);
        }

        // class-code
        const cf_object cls_ref(::IORegistryEntryCreateCFProperty(
            service, CFSTR("class-code"), ::kCFAllocatorDefault, 0));
        const auto cls_val = optional_cfdata_u32(cls_ref.get());
        if (!cls_val) { return fail(cls_val.error()); }
        if (*cls_val) {
                dev.programming_interface = static_cast<std::uint8_t>(**cls_val & 0xFFU);
                dev.subclass_code = static_cast<std::uint8_t>((**cls_val >> 8U) & 0xFFU);
                dev.class_code = static_cast<std::uint8_t>((**cls_val >> 16U) & 0xFFU);
                dev.device_class = hardware_common::classify_pci_class(*dev.class_code);
        }

        // assigned-addresses (bus / device / function)
        const cf_object addr_ref(::IORegistryEntryCreateCFProperty(
            service, CFSTR("assigned-addresses"), ::kCFAllocatorDefault, 0));
        const auto addr_val = optional_assigned_address_phys_hi(addr_ref.get());
        if (!addr_val) { return fail(addr_val.error()); }
        if (*addr_val) {
            dev.bus = static_cast<std::uint8_t>((**addr_val >> 16U) & 0xFFU);
            dev.device = static_cast<std::uint8_t>((**addr_val >> 11U) & 0x1FU);
            dev.function = static_cast<std::uint8_t>((**addr_val >> 8U) & 0x07U);
        }

        result_devices.push_back(std::move(dev));
    }

    std::sort(result_devices.begin(), result_devices.end(), hardware_common::compare_pci_devices);
    return result_devices;
}

inline result<std::vector<::syscape::hardware::usb_device>> usb_devices_for_class(
    const char* service_class) {
#if defined(kIOMainPortDefault)
    const ::io_service_t port = kIOMainPortDefault;
#else
    const ::io_service_t port = kIOMasterPortDefault;
#endif
    ::CFMutableDictionaryRef matching = ::IOServiceMatching(service_class);
    if (matching == nullptr) { return fail(errc::io_error); }
    ::io_iterator_t iterator = IO_OBJECT_NULL;
    if (::IOServiceGetMatchingServices(port, matching, &iterator) != KERN_SUCCESS ||
        iterator == IO_OBJECT_NULL) {
        return fail(errc::io_error);
    }
    const io_object_guard iter_guard(iterator);

    std::vector<::syscape::hardware::usb_device> result_devices;
    ::io_service_t service = IO_OBJECT_NULL;
    while ((service = ::IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        const io_object_guard service_guard(service);
        ::syscape::hardware::usb_device dev;

        // idVendor
        const cf_object ven_ref(::IORegistryEntryCreateCFProperty(
            service, CFSTR("idVendor"), ::kCFAllocatorDefault, 0));
        const auto ven_val = optional_cfnumber_u64(ven_ref.get());
        if (!ven_val) { return fail(ven_val.error()); }

        // idProduct
        const cf_object prod_ref(::IORegistryEntryCreateCFProperty(
            service, CFSTR("idProduct"), ::kCFAllocatorDefault, 0));
        const auto prod_val = optional_cfnumber_u64(prod_ref.get());
        if (!prod_val) { return fail(prod_val.error()); }
        if (!*ven_val || !*prod_val) { continue; }
        if (**ven_val > 0xFFFFU || **prod_val > 0xFFFFU) {
            return fail(errc::malformed_data);
        }
        dev.vendor_id = static_cast<std::uint16_t>(**ven_val);
        dev.product_id = static_cast<std::uint16_t>(**prod_val);

        // bcdDevice
        const cf_object bcd_ref(::IORegistryEntryCreateCFProperty(
            service, CFSTR("bcdDevice"), ::kCFAllocatorDefault, 0));
        const auto bcd_val = optional_cfnumber_u64(bcd_ref.get());
        if (!bcd_val) { return fail(bcd_val.error()); }
        if (*bcd_val) {
            if (**bcd_val > 0xFFFFU) { return fail(errc::malformed_data); }
            dev.bcd_device = static_cast<std::uint16_t>(**bcd_val);
        }

        // bDeviceClass
        const cf_object cls_ref(::IORegistryEntryCreateCFProperty(
            service, CFSTR("bDeviceClass"), ::kCFAllocatorDefault, 0));
        const auto usb_class = optional_cfnumber_u64(cls_ref.get());
        if (!usb_class) { return fail(usb_class.error()); }
        if (*usb_class) {
            if (**usb_class > 0xFFU) { return fail(errc::malformed_data); }
            dev.device_class = static_cast<std::uint8_t>(**usb_class);
        }

        // bDeviceSubClass
        const cf_object sub_ref(::IORegistryEntryCreateCFProperty(
            service, CFSTR("bDeviceSubClass"), ::kCFAllocatorDefault, 0));
        const auto usb_subclass = optional_cfnumber_u64(sub_ref.get());
        if (!usb_subclass) { return fail(usb_subclass.error()); }
        if (*usb_subclass) {
            if (**usb_subclass > 0xFFU) { return fail(errc::malformed_data); }
            dev.device_subclass = static_cast<std::uint8_t>(**usb_subclass);
        }

        // bDeviceProtocol
        const cf_object proto_ref(::IORegistryEntryCreateCFProperty(
            service, CFSTR("bDeviceProtocol"), ::kCFAllocatorDefault, 0));
        const auto usb_protocol = optional_cfnumber_u64(proto_ref.get());
        if (!usb_protocol) { return fail(usb_protocol.error()); }
        if (*usb_protocol) {
            if (**usb_protocol > 0xFFU) { return fail(errc::malformed_data); }
            dev.device_protocol = static_cast<std::uint8_t>(**usb_protocol);
        }

        // USB Vendor Name
        const cf_object mfg_ref(::IORegistryEntryCreateCFProperty(
            service, CFSTR("USB Vendor Name"), ::kCFAllocatorDefault, 0));
        const auto utf8_mfg = optional_cfstring_utf8(mfg_ref.get());
        if (!utf8_mfg) { return fail(utf8_mfg.error()); }
        if (*utf8_mfg) { dev.manufacturer = **utf8_mfg; }

        // USB Product Name
        const cf_object pname_ref(::IORegistryEntryCreateCFProperty(
            service, CFSTR("USB Product Name"), ::kCFAllocatorDefault, 0));
        const auto utf8_pname = optional_cfstring_utf8(pname_ref.get());
        if (!utf8_pname) { return fail(utf8_pname.error()); }
        if (*utf8_pname) { dev.product = **utf8_pname; }

        // USB Serial Number
        const cf_object sn_ref(::IORegistryEntryCreateCFProperty(
            service, CFSTR("USB Serial Number"), ::kCFAllocatorDefault, 0));
        const auto utf8_sn = optional_cfstring_utf8(sn_ref.get());
        if (!utf8_sn) { return fail(utf8_sn.error()); }
        if (*utf8_sn) { dev.serial_number = **utf8_sn; }

        // USB Address
        const cf_object address_ref(::IORegistryEntryCreateCFProperty(
            service, CFSTR("USB Address"), ::kCFAllocatorDefault, 0));
        const auto address = optional_cfnumber_u64(address_ref.get());
        if (!address) { return fail(address.error()); }
        if (*address) {
            if (**address > 127U) { return fail(errc::malformed_data); }
            dev.device_address = static_cast<std::uint8_t>(**address);
        }

        // locationID / PortNum
        const cf_object loc_ref(::IORegistryEntryCreateCFProperty(
            service, CFSTR("locationID"), ::kCFAllocatorDefault, 0));
        const auto loc_val = optional_cfnumber_u64(loc_ref.get());
        if (!loc_val) { return fail(loc_val.error()); }
        if (*loc_val) {
            if (**loc_val > 0xFFFFFFFFULL) { return fail(errc::malformed_data); }
            dev.bus_number = static_cast<std::uint8_t>((**loc_val >> 24U) & 0xFFU);
            std::uint8_t immediate_port = 0U;
            for (unsigned int shift = 20U;; shift -= 4U) {
                const std::uint8_t component =
                    static_cast<std::uint8_t>((**loc_val >> shift) & 0x0FU);
                if (component != 0U) { immediate_port = component; }
                if (shift == 0U) { break; }
            }
            if (immediate_port != 0U) {
                dev.port_number = immediate_port;
            }
        }

        result_devices.push_back(std::move(dev));
    }

    std::sort(result_devices.begin(), result_devices.end(), hardware_common::compare_usb_devices);
    return result_devices;
}

inline result<std::vector<::syscape::hardware::usb_device>> usb_devices() {
    result<std::vector<::syscape::hardware::usb_device>> devices =
        usb_devices_for_class("IOUSBHostDevice");
    if (!devices) { return fail(devices.error()); }
    if (!devices->empty()) { return devices; }
    return usb_devices_for_class("IOUSBDevice");
}

inline result<std::vector<::syscape::hardware::memory_device>> memory_devices() {
    return fail(errc::not_supported);
}

} // namespace hardware_backend
} // namespace detail
} // namespace syscape

#endif
