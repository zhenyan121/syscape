#ifndef SYSCAPE_DETAIL_HARDWARE_MACOS_HPP
#define SYSCAPE_DETAIL_HARDWARE_MACOS_HPP

#include <string>

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

/// Copies one recorded registry string property into plain storage.
///
/// An absent key records an absent field because platforms legitimately omit
/// individual identity properties. A present value of a foreign type
/// contradicts the documented property contract and fails as malformed
/// platform data instead of being coerced or skipped silently.
inline result<void> read_property(::io_service_t service, ::CFStringRef key,
                                  bool& present, std::string& destination) {
    const cf_object reference(::IORegistryEntryCreateCFProperty(
        service, key, ::kCFAllocatorDefault, 0));
    if (reference.get() == nullptr) { return {}; }
    if (::CFGetTypeID(reference.get()) != ::CFStringGetTypeID()) {
        return fail(errc::malformed_data);
    }
    result<std::string> text = copy_utf8_string(
        static_cast<::CFStringRef>(reference.get()));
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
                      facts.has_manufacturer, facts.manufacturer);
    if (!stored_manufacturer) { return fail(stored_manufacturer.error()); }
    const result<void> stored_model =
        read_property(service, CFSTR("model"), facts.has_product_name,
                      facts.product_name);
    if (!stored_model) { return fail(stored_model.error()); }
    const result<void> stored_board =
        read_property(service, CFSTR("board-id"), facts.has_board_product,
                      facts.board_product);
    if (!stored_board) { return fail(stored_board.error()); }
    const result<void> stored_uuid =
        read_property(service, CFSTR("IOPlatformUUID"), facts.has_uuid,
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

} // namespace hardware_backend
} // namespace detail
} // namespace syscape

#endif
