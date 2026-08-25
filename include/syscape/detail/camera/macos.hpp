#ifndef SYSCAPE_DETAIL_CAMERA_MACOS_HPP
#define SYSCAPE_DETAIL_CAMERA_MACOS_HPP

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMediaIO/CMIOHardware.h>

#include <syscape/camera.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace camera_backend {

class osstatus_error_category final : public std::error_category {
public:
    const char* name() const noexcept override { return "macos-osstatus"; }

    std::string message(int value) const override {
        return "macOS OSStatus " + std::to_string(value);
    }
};

inline const std::error_category& osstatus_category() noexcept {
    static const osstatus_error_category category;
    return category;
}

inline std::error_code map_osstatus(OSStatus value) noexcept {
    return std::error_code(static_cast<int>(value), osstatus_category());
}

class cf_object {
public:
    explicit cf_object(::CFTypeRef value) noexcept : value_(value) {}
    cf_object(const cf_object&) = delete;
    cf_object& operator=(const cf_object&) = delete;
    ~cf_object() {
        if (value_ != nullptr) {
            ::CFRelease(value_);
        }
    }
    ::CFTypeRef get() const noexcept { return value_; }

private:
    ::CFTypeRef value_;
};

inline result<std::string> copy_utf8_string(::CFStringRef value) {
    if (value == nullptr) {
        return fail(errc::io_error);
    }
    const ::CFIndex length = ::CFStringGetLength(value);
    const ::CFIndex maximum =
        ::CFStringGetMaximumSizeForEncoding(length, ::kCFStringEncodingUTF8);
    if (maximum < 0 || maximum == (std::numeric_limits<::CFIndex>::max)()) {
        return fail(errc::value_too_large);
    }
    const auto maximum_size = static_cast<unsigned long long>(maximum);
    const auto size_limit = static_cast<unsigned long long>(
        (std::numeric_limits<std::size_t>::max)());
    if (maximum_size > size_limit - 1U) {
        return fail(errc::value_too_large);
    }
    const std::size_t buffer_size = static_cast<std::size_t>(maximum_size) + 1U;
    std::vector<char> buffer;
    if (buffer_size > buffer.max_size()) {
        return fail(errc::value_too_large);
    }
    buffer.resize(buffer_size);
    if (!::CFStringGetCString(value, buffer.data(),
                              static_cast<::CFIndex>(buffer_size),
                              kCFStringEncodingUTF8)) {
        return fail(errc::invalid_encoding);
    }
    const std::string converted(buffer.data());
    if (!is_valid_utf8(converted)) {
        return fail(errc::invalid_encoding);
    }
    return converted;
}

inline result<std::string>
get_cmio_string_property(CMIOObjectID object_id,
                         const CMIOObjectPropertyAddress& address) {
    CFStringRef str_ref = nullptr;
    UInt32 data_size = sizeof(CFStringRef);
    const OSStatus status =
        ::CMIOObjectGetPropertyData(object_id, &address, 0, nullptr,
                                    sizeof(CFStringRef), &data_size, &str_ref);
    if (status != noErr) {
        return fail(map_osstatus(status));
    }
    if (str_ref == nullptr) {
        return fail(errc::malformed_data);
    }
    const cf_object owned(str_ref);
    if (data_size != sizeof(CFStringRef) ||
        ::CFGetTypeID(str_ref) != ::CFStringGetTypeID()) {
        return fail(errc::malformed_data);
    }
    return copy_utf8_string(str_ref);
}

// CoreMediaIO property element zero is the main element. Using the value
// directly keeps this backend compatible with SDKs from before the
// kCMIOObjectPropertyElementMain spelling was introduced, without referring
// to the deprecated kCMIOObjectPropertyElementMaster alias on newer SDKs.
constexpr CMIOObjectPropertyElement camera_property_element_main = 0U;

inline result<std::size_t>
cmio_device_count_from_size(UInt32 data_size) noexcept {
    if (data_size % sizeof(CMIOObjectID) != 0U) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::size_t>(data_size / sizeof(CMIOObjectID));
}

inline result<std::vector<::syscape::camera::camera_device>>
enumerate_cmio_devices() {
    CMIOObjectPropertyAddress prop_address;
    prop_address.mSelector = kCMIOHardwarePropertyDevices;
    prop_address.mScope = kCMIOObjectPropertyScopeGlobal;
    prop_address.mElement = camera_property_element_main;

    UInt32 data_size = 0U;
    OSStatus status = ::CMIOObjectGetPropertyDataSize(
        kCMIOObjectSystemObject, &prop_address, 0, nullptr, &data_size);
    if (status != noErr) {
        return fail(map_osstatus(status));
    }

    const auto count_result = cmio_device_count_from_size(data_size);
    if (!count_result) {
        return fail(count_result.error());
    }
    const std::size_t count = *count_result;
    if (count == 0U) {
        return std::vector<::syscape::camera::camera_device>{};
    }

    std::vector<CMIOObjectID> device_ids(count);
    const UInt32 buffer_size = data_size;
    status = ::CMIOObjectGetPropertyData(kCMIOObjectSystemObject, &prop_address,
                                         0, nullptr, buffer_size, &data_size,
                                         device_ids.data());
    if (status != noErr) {
        return fail(map_osstatus(status));
    }
    if (data_size > buffer_size || data_size % sizeof(CMIOObjectID) != 0U) {
        return fail(errc::malformed_data);
    }
    device_ids.resize(data_size / sizeof(CMIOObjectID));

    std::vector<::syscape::camera::camera_device> devices;
    devices.reserve(count);

    for (CMIOObjectID obj_id : device_ids) {
        ::syscape::camera::camera_device dev;
        CMIOObjectPropertyAddress name_addr;
        name_addr.mSelector = kCMIOObjectPropertyName;
        name_addr.mScope = kCMIOObjectPropertyScopeGlobal;
        name_addr.mElement = camera_property_element_main;

        const auto name_res = get_cmio_string_property(obj_id, name_addr);
        if (!name_res) {
            return fail(name_res.error());
        }
        dev.name = *name_res;

        CMIOObjectPropertyAddress uid_addr;
        uid_addr.mSelector = kCMIODevicePropertyDeviceUID;
        uid_addr.mScope = kCMIOObjectPropertyScopeGlobal;
        uid_addr.mElement = camera_property_element_main;

        const auto uid_res = get_cmio_string_property(obj_id, uid_addr);
        if (!uid_res) {
            return fail(uid_res.error());
        }
        dev.id = *uid_res;

        CMIOObjectPropertyAddress model_addr;
        model_addr.mSelector = kCMIODevicePropertyModelUID;
        model_addr.mScope = kCMIOObjectPropertyScopeGlobal;
        model_addr.mElement = camera_property_element_main;

        if (::CMIOObjectHasProperty(obj_id, &model_addr)) {
            const auto model_res = get_cmio_string_property(obj_id, model_addr);
            if (!model_res) {
                return fail(model_res.error());
            }
            dev.card = *model_res;
        }

        devices.push_back(std::move(dev));
    }

    return devices;
}

inline result<std::vector<::syscape::camera::camera_device>> devices() {
    return enumerate_cmio_devices();
}

inline result<std::size_t> device_count() {
    const auto res = devices();
    if (!res) {
        return fail(res.error());
    }
    return res->size();
}

inline result<std::vector<::syscape::camera::camera_device>> capture_devices() {
    return fail(errc::not_supported);
}

inline result<::syscape::camera::camera_device> default_device() {
    return fail(errc::not_supported);
}

} // namespace camera_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_CAMERA_MACOS_HPP
