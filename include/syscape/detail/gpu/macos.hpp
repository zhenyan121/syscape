#ifndef SYSCAPE_DETAIL_GPU_MACOS_HPP
#define SYSCAPE_DETAIL_GPU_MACOS_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#include <syscape/detail/gpu/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/gpu.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace gpu_backend {

class cf_object {
public:
    explicit cf_object(::CFTypeRef value) noexcept : value_(value) {}
    cf_object(const cf_object&) = delete;
    cf_object& operator=(const cf_object&) = delete;
    ~cf_object() {
        if (value_ != nullptr) { ::CFRelease(value_); }
    }
    ::CFTypeRef get() const noexcept { return value_; }

private:
    ::CFTypeRef value_;
};

class io_object_guard {
public:
    explicit io_object_guard(::io_object_t value) noexcept : value_(value) {}
    io_object_guard(const io_object_guard&) = delete;
    io_object_guard& operator=(const io_object_guard&) = delete;
    ~io_object_guard() {
        if (value_ != IO_OBJECT_NULL) { ::IOObjectRelease(value_); }
    }
    ::io_object_t get() const noexcept { return value_; }

private:
    ::io_object_t value_;
};

inline result<std::string> copy_utf8_string(::CFStringRef value) {
    if (value == nullptr) { return fail(errc::io_error); }
    const ::CFIndex length = ::CFStringGetLength(value);
    const ::CFIndex maximum = ::CFStringGetMaximumSizeForEncoding(
        length, ::kCFStringEncodingUTF8);
    if (maximum < 0) { return fail(errc::io_error); }
    std::string output;
    output.resize(static_cast<std::size_t>(maximum) + 1U);
    if (!::CFStringGetCString(value, output.data(), maximum + 1,
                              ::kCFStringEncodingUTF8)) {
        return fail(errc::invalid_encoding);
    }
    output.resize(std::char_traits<char>::length(output.c_str()));
    if (!is_valid_utf8(output)) {
        return fail(errc::invalid_encoding);
    }
    return output;
}

inline result<std::optional<std::uint32_t>> extract_u32_property(::CFTypeRef prop) {
    if (prop == nullptr) {
        return std::optional<std::uint32_t>(std::nullopt);
    }
    if (::CFGetTypeID(prop) == ::CFNumberGetTypeID()) {
        std::uint32_t val = 0U;
        if (!::CFNumberGetValue(static_cast<::CFNumberRef>(prop), kCFNumberSInt32Type, &val)) {
            return fail(errc::malformed_data);
        }
        return std::optional<std::uint32_t>(val);
    }
    if (::CFGetTypeID(prop) == ::CFDataGetTypeID()) {
        const auto data = static_cast<::CFDataRef>(prop);
        const auto len = ::CFDataGetLength(data);
        if (len != 4 && len != 2 && len != 1) {
            return fail(errc::malformed_data);
        }
        const auto* bytes = ::CFDataGetBytePtr(data);
        if (bytes == nullptr) {
            return fail(errc::malformed_data);
        }
        std::uint32_t val = 0U;
        for (::CFIndex i = 0; i < len; ++i) {
            val |= (static_cast<std::uint32_t>(bytes[i]) << (8U * static_cast<unsigned>(i)));
        }
        return std::optional<std::uint32_t>(val);
    }
    return fail(errc::malformed_data);
}

inline result<std::optional<std::string>> extract_string_property(::CFTypeRef prop) {
    if (prop == nullptr) { return std::optional<std::string>(std::nullopt); }
    if (::CFGetTypeID(prop) == ::CFStringGetTypeID()) {
        const auto res = copy_utf8_string(static_cast<::CFStringRef>(prop));
        if (!res) { return fail(res.error()); }
        return std::optional<std::string>(*res);
    }
    if (::CFGetTypeID(prop) == ::CFDataGetTypeID()) {
        const auto data = static_cast<::CFDataRef>(prop);
        const char* ptr = reinterpret_cast<const char*>(::CFDataGetBytePtr(data));
        const ::CFIndex len = ::CFDataGetLength(data);
        if (ptr != nullptr && len > 0) {
            std::string raw(ptr, static_cast<std::size_t>(len));
            while (!raw.empty() && raw.back() == '\0') {
                raw.pop_back();
            }
            if (!is_valid_utf8(raw)) {
                return fail(errc::invalid_encoding);
            }
            return std::optional<std::string>(std::move(raw));
        }
    }
    return std::optional<std::string>(std::nullopt);
}

inline result<std::vector<::syscape::gpu::gpu_device>> collect_devices() {
    std::vector<::syscape::gpu::gpu_device> list;
    std::vector<uint64_t> seen_entry_ids;

    // 1. Match IOPCIDevice services for PCI graphics cards
    ::CFMutableDictionaryRef pci_matching = ::IOServiceMatching("IOPCIDevice");
    if (pci_matching == nullptr) {
        return fail(errc::io_error);
    }
    ::io_iterator_t pci_iterator = IO_OBJECT_NULL;
    const ::kern_return_t pci_rc = ::IOServiceGetMatchingServices(
        MACH_PORT_NULL, pci_matching, &pci_iterator);
    if (pci_rc != KERN_SUCCESS) {
        return fail(errc::io_error);
    }
    const io_object_guard pci_guard(pci_iterator);

    for (;;) {
        const ::io_service_t service = ::IOIteratorNext(pci_iterator);
        if (service == IO_OBJECT_NULL) { break; }
        const io_object_guard service_guard(service);

        // Check class-code (PCI display class 0x03)
        const cf_object class_prop(::IORegistryEntryCreateCFProperty(
            service, CFSTR("class-code"), kCFAllocatorDefault, 0));
        if (class_prop.get() != nullptr) {
            if (::CFGetTypeID(class_prop.get()) != ::CFDataGetTypeID()) {
                return fail(errc::malformed_data);
            }
            const auto class_data = static_cast<::CFDataRef>(class_prop.get());
            if (::CFDataGetLength(class_data) != 4) {
                return fail(errc::malformed_data);
            }
            const auto* bytes = ::CFDataGetBytePtr(class_data);
            if (bytes == nullptr) {
                return fail(errc::malformed_data);
            }
            if (bytes[2] == 0x03U) {
                ::syscape::gpu::gpu_device dev;
                io_name_t name_buffer;
                if (::IORegistryEntryGetName(service, name_buffer) != KERN_SUCCESS) {
                    return fail(errc::io_error);
                }
                dev.id = name_buffer;
                if (dev.id.empty()) {
                    return fail(errc::malformed_data);
                }
                if (!is_valid_utf8(dev.id)) {
                    return fail(errc::invalid_encoding);
                }

                uint64_t pci_entry_id = 0U;
                if (::IORegistryEntryGetRegistryEntryID(
                        service, &pci_entry_id) != KERN_SUCCESS) {
                    return fail(errc::io_error);
                }
                seen_entry_ids.push_back(pci_entry_id);

                const cf_object ven_prop(::IORegistryEntryCreateCFProperty(
                    service, CFSTR("vendor-id"), kCFAllocatorDefault, 0));
                const auto ven_res = extract_u32_property(ven_prop.get());
                if (!ven_res) { return fail(ven_res.error()); }
                dev.vendor_id = *ven_res;

                const cf_object did_prop(::IORegistryEntryCreateCFProperty(
                    service, CFSTR("device-id"), kCFAllocatorDefault, 0));
                const auto did_res = extract_u32_property(did_prop.get());
                if (!did_res) { return fail(did_res.error()); }
                dev.device_id = *did_res;

                const cf_object model_prop(::IORegistryEntryCreateCFProperty(
                    service, CFSTR("model"), kCFAllocatorDefault, 0));
                const auto name_res = extract_string_property(model_prop.get());
                if (!name_res) { return fail(name_res.error()); }
                dev.name = *name_res;

                const cf_object boot_disp(::IORegistryEntryCreateCFProperty(
                    service, CFSTR("boot-display"), kCFAllocatorDefault, 0));
                if (boot_disp.get() != nullptr) {
                    dev.is_primary = true;
                }

                if (dev.vendor_id.has_value()) {
                    dev.vendor = gpu_common::classify_pci_vendor_id(*dev.vendor_id);
                } else if (dev.name.has_value() && !dev.name->empty()) {
                    dev.vendor = gpu_common::classify_vendor_name(*dev.name);
                } else {
                    dev.vendor = ::syscape::gpu::gpu_vendor::unknown;
                }
                dev.vendor_name = gpu_common::vendor_to_string(dev.vendor);

                list.push_back(std::move(dev));
            }
        }
    }

    // 2. Match IOAccelerator services (covers Apple Silicon and unified accelerators)
    ::CFMutableDictionaryRef accel_matching = ::IOServiceMatching("IOAccelerator");
    if (accel_matching == nullptr) {
        return fail(errc::io_error);
    }
    ::io_iterator_t accel_iterator = IO_OBJECT_NULL;
    const ::kern_return_t accel_rc = ::IOServiceGetMatchingServices(
        MACH_PORT_NULL, accel_matching, &accel_iterator);
    if (accel_rc != KERN_SUCCESS) {
        return fail(errc::io_error);
    }
    const io_object_guard accel_guard(accel_iterator);

    for (;;) {
        const ::io_service_t service = ::IOIteratorNext(accel_iterator);
        if (service == IO_OBJECT_NULL) { break; }
        const io_object_guard service_guard(service);

        // Check if this accelerator is a child/descendant of an IOPCIDevice already collected
        bool is_child_of_seen_pci = false;
        ::io_registry_entry_t current = service;
        if (::IOObjectRetain(current) != KERN_SUCCESS) {
            return fail(errc::io_error);
        }
        for (;;) {
            ::io_registry_entry_t parent = IO_OBJECT_NULL;
            if (::IORegistryEntryGetParentEntry(current, kIOServicePlane, &parent) != KERN_SUCCESS) {
                break;
            }
            ::IOObjectRelease(current);
            current = parent;
            uint64_t parent_id = 0U;
            if (::IORegistryEntryGetRegistryEntryID(current, &parent_id) != KERN_SUCCESS) {
                ::IOObjectRelease(current);
                return fail(errc::io_error);
            }
            for (uint64_t seen_id : seen_entry_ids) {
                if (parent_id == seen_id) {
                    is_child_of_seen_pci = true;
                    break;
                }
            }
            if (is_child_of_seen_pci) {
                break;
            }
        }
        ::IOObjectRelease(current);
        if (is_child_of_seen_pci) {
            continue;
        }

        ::syscape::gpu::gpu_device dev;
        io_name_t name_buffer;
        if (::IORegistryEntryGetName(service, name_buffer) != KERN_SUCCESS) {
            return fail(errc::io_error);
        }
        dev.id = name_buffer;
        if (dev.id.empty()) {
            return fail(errc::malformed_data);
        }
        if (!is_valid_utf8(dev.id)) {
            return fail(errc::invalid_encoding);
        }

        const cf_object model_prop(::IORegistryEntryCreateCFProperty(
            service, CFSTR("model"), kCFAllocatorDefault, 0));
        const auto name_res = extract_string_property(model_prop.get());
        if (!name_res) { return fail(name_res.error()); }
        dev.name = *name_res;

        const cf_object boot_disp(::IORegistryEntryCreateCFProperty(
            service, CFSTR("boot-display"), kCFAllocatorDefault, 0));
        if (boot_disp.get() != nullptr) {
            dev.is_primary = true;
        }

        if (dev.name.has_value() && !dev.name->empty()) {
            dev.vendor = gpu_common::classify_vendor_name(*dev.name);
        } else {
            dev.vendor = ::syscape::gpu::gpu_vendor::unknown;
        }
        dev.vendor_name = gpu_common::vendor_to_string(dev.vendor);

        list.push_back(std::move(dev));
    }

    return list;
}

inline result<std::vector<::syscape::gpu::gpu_device>> devices() {
    return collect_devices();
}

inline result<std::size_t> device_count() {
    const auto res = collect_devices();
    if (!res) { return fail(res.error()); }
    return res->size();
}

inline result<::syscape::gpu::gpu_device> primary_device() {
    const auto res = collect_devices();
    if (!res) { return fail(res.error()); }
    if (res->empty()) { return fail(errc::not_found); }

    for (const auto& dev : *res) {
        if (dev.is_primary.has_value() && *dev.is_primary) {
            return dev;
        }
    }
    return fail(errc::not_found);
}

} // namespace gpu_backend
} // namespace detail
} // namespace syscape

#endif
