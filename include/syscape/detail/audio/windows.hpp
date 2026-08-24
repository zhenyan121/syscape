#ifndef SYSCAPE_DETAIL_AUDIO_WINDOWS_HPP
#define SYSCAPE_DETAIL_AUDIO_WINDOWS_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <objbase.h>
#include <propsys.h>
#include <propvarutil.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <syscape/audio.hpp>
#include <syscape/detail/audio/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace audio_backend {

class hresult_error_category final : public std::error_category {
public:
    const char* name() const noexcept override { return "windows-hresult"; }

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

inline const std::error_category& hresult_category() noexcept {
    static const hresult_error_category category;
    return category;
}

inline std::error_code map_hresult(HRESULT value) noexcept {
    if (value == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) {
        return make_error_code(errc::not_found);
    }
    if (HRESULT_FACILITY(value) == FACILITY_WIN32) {
        return std::error_code(static_cast<int>(HRESULT_CODE(value)),
                               std::system_category());
    }
    return std::error_code(static_cast<int>(value), hresult_category());
}

template <typename T>
class com_ptr {
public:
    com_ptr() noexcept : ptr_(nullptr) {}
    explicit com_ptr(T* p) noexcept : ptr_(p) {}
    ~com_ptr() {
        if (ptr_ != nullptr) {
            ptr_->Release();
        }
    }
    com_ptr(const com_ptr&) = delete;
    com_ptr& operator=(const com_ptr&) = delete;
    com_ptr(com_ptr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    com_ptr& operator=(com_ptr&& other) noexcept {
        if (this != &other) {
            if (ptr_ != nullptr) {
                ptr_->Release();
            }
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    T* get() const noexcept { return ptr_; }
    T** put() noexcept {
        if (ptr_ != nullptr) {
            ptr_->Release();
            ptr_ = nullptr;
        }
        return &ptr_;
    }
    void** put_void() noexcept {
        return reinterpret_cast<void**>(put());
    }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

private:
    T* ptr_ = nullptr;
};

class co_init_guard {
public:
    co_init_guard() noexcept {
        hr_ = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (hr_ == RPC_E_CHANGED_MODE) {
            hr_ = S_OK;
            initialized_ = false;
        } else if (SUCCEEDED(hr_)) {
            initialized_ = true;
        }
    }
    ~co_init_guard() {
        if (initialized_) {
            ::CoUninitialize();
        }
    }
    bool ok() const noexcept { return SUCCEEDED(hr_); }
    HRESULT error() const noexcept { return hr_; }

private:
    HRESULT hr_ = E_FAIL;
    bool initialized_ = false;
};

class prop_variant_guard {
public:
    prop_variant_guard() noexcept { ::PropVariantInit(&value_); }
    ~prop_variant_guard() { ::PropVariantClear(&value_); }
    prop_variant_guard(const prop_variant_guard&) = delete;
    prop_variant_guard& operator=(const prop_variant_guard&) = delete;
    PROPVARIANT* get() noexcept { return &value_; }
    const PROPVARIANT* get() const noexcept { return &value_; }

private:
    PROPVARIANT value_{};
};

class task_mem_guard {
public:
    explicit task_mem_guard(LPWSTR str) noexcept : str_(str) {}
    ~task_mem_guard() {
        if (str_ != nullptr) {
            ::CoTaskMemFree(str_);
        }
    }
    task_mem_guard(const task_mem_guard&) = delete;
    task_mem_guard& operator=(const task_mem_guard&) = delete;
    LPWSTR get() const noexcept { return str_; }

private:
    LPWSTR str_ = nullptr;
};

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

inline result<std::wstring> default_endpoint_id(
    IMMDeviceEnumerator& enumerator, EDataFlow flow) {
    com_ptr<IMMDevice> endpoint;
    const HRESULT endpoint_result = enumerator.GetDefaultAudioEndpoint(
        flow, eConsole, endpoint.put());
    if (FAILED(endpoint_result) || !endpoint) {
        return fail(FAILED(endpoint_result)
                        ? map_hresult(endpoint_result)
                        : make_error_code(errc::malformed_data));
    }
    LPWSTR raw_id = nullptr;
    const HRESULT id_result = endpoint->GetId(&raw_id);
    if (FAILED(id_result) || raw_id == nullptr) {
        return fail(FAILED(id_result) ? map_hresult(id_result)
                                      : make_error_code(errc::malformed_data));
    }
    task_mem_guard guard(raw_id);
    return std::wstring(raw_id);
}

inline result<std::vector<::syscape::audio::audio_device>> collect_devices(
    std::optional<EDataFlow> required_default = std::nullopt) {
    co_init_guard com;
    if (!com.ok()) {
        return fail(map_hresult(com.error()));
    }

    com_ptr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = ::CoCreateInstance(
        CLSID_MMDeviceEnumerator, nullptr, CLSCTX_INPROC_SERVER,
        IID_IMMDeviceEnumerator, enumerator.put_void());
    if (FAILED(hr) || !enumerator) {
        return fail(FAILED(hr) ? map_hresult(hr)
                               : make_error_code(errc::not_supported));
    }

    const auto default_render_id = default_endpoint_id(*enumerator.get(), eRender);
    const auto default_capture_id = default_endpoint_id(*enumerator.get(), eCapture);
    if (required_default == eRender && !default_render_id) {
        return fail(default_render_id.error());
    }
    if (required_default == eCapture && !default_capture_id) {
        return fail(default_capture_id.error());
    }

    com_ptr<IMMDeviceCollection> collection;
    hr = enumerator->EnumAudioEndpoints(eAll, DEVICE_STATE_ACTIVE, collection.put());
    if (FAILED(hr) || !collection) {
        return fail(FAILED(hr) ? map_hresult(hr)
                               : make_error_code(errc::io_error));
    }

    UINT count = 0U;
    hr = collection->GetCount(&count);
    if (FAILED(hr)) {
        return fail(map_hresult(hr));
    }

    std::vector<::syscape::audio::audio_device> devices;
    devices.reserve(count);

    for (UINT i = 0U; i < count; ++i) {
        com_ptr<IMMDevice> dev;
        hr = collection->Item(i, dev.put());
        if (FAILED(hr) || !dev) {
            return fail(FAILED(hr) ? map_hresult(hr)
                                   : make_error_code(errc::malformed_data));
        }

        LPWSTR wstr_id = nullptr;
        hr = dev->GetId(&wstr_id);
        if (FAILED(hr) || wstr_id == nullptr) {
            return fail(FAILED(hr) ? map_hresult(hr)
                                   : make_error_code(errc::malformed_data));
        }
        task_mem_guard id_guard(wstr_id);

        const std::wstring raw_id(wstr_id);
        const auto id_utf8 = wide_to_utf8(raw_id);
        if (!id_utf8) {
            return fail(id_utf8.error());
        }

        ::syscape::audio::audio_device audio_dev;
        audio_dev.id = *id_utf8;
        audio_dev.state = ::syscape::audio::audio_device_state::active;

        com_ptr<IMMEndpoint> endpoint;
        hr = dev->QueryInterface(IID_IMMEndpoint, endpoint.put_void());
        if (FAILED(hr) || !endpoint) {
            return fail(FAILED(hr) ? map_hresult(hr)
                                   : make_error_code(errc::malformed_data));
        }
        EDataFlow flow;
        hr = endpoint->GetDataFlow(&flow);
        if (FAILED(hr)) {
            return fail(map_hresult(hr));
        }
        if (flow == eRender) {
            audio_dev.direction = ::syscape::audio::audio_device_direction::playback;
        } else if (flow == eCapture) {
            audio_dev.direction = ::syscape::audio::audio_device_direction::capture;
        } else {
            return fail(errc::malformed_data);
        }

        if (default_render_id) {
            audio_dev.is_default_playback = raw_id == *default_render_id;
        }
        if (default_capture_id) {
            audio_dev.is_default_capture = raw_id == *default_capture_id;
        }

        com_ptr<IPropertyStore> props;
        hr = dev->OpenPropertyStore(STGM_READ, props.put());
        if (FAILED(hr) || !props) {
            return fail(FAILED(hr) ? map_hresult(hr)
                                   : make_error_code(errc::malformed_data));
        }
        prop_variant_guard prop_name;
        hr = props->GetValue(PKEY_Device_FriendlyName, prop_name.get());
        if (FAILED(hr)) {
            return fail(map_hresult(hr));
        }
        if (prop_name.get()->vt != VT_LPWSTR ||
            prop_name.get()->pwszVal == nullptr) {
            return fail(errc::malformed_data);
        }
        const auto name_utf8 = wide_to_utf8(prop_name.get()->pwszVal);
        if (!name_utf8) {
            return fail(name_utf8.error());
        }
        audio_dev.name = *name_utf8;

        prop_variant_guard prop_fmt;
        hr = props->GetValue(PKEY_AudioEngine_DeviceFormat, prop_fmt.get());
        if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) {
            return fail(map_hresult(hr));
        }
        if (SUCCEEDED(hr) && prop_fmt.get()->vt != VT_EMPTY) {
            if (prop_fmt.get()->vt != VT_BLOB ||
                prop_fmt.get()->blob.cbSize < sizeof(WAVEFORMATEX) ||
                prop_fmt.get()->blob.pBlobData == nullptr) {
                return fail(errc::malformed_data);
            }
            WAVEFORMATEX format{};
            std::memcpy(&format, prop_fmt.get()->blob.pBlobData,
                        sizeof(format));
            if (format.nChannels == 0U || format.nSamplesPerSec == 0U) {
                return fail(errc::malformed_data);
            }
            if (audio_dev.direction ==
                ::syscape::audio::audio_device_direction::playback) {
                audio_dev.playback_channels =
                    static_cast<std::uint32_t>(format.nChannels);
            } else if (audio_dev.direction ==
                       ::syscape::audio::audio_device_direction::capture) {
                audio_dev.capture_channels =
                    static_cast<std::uint32_t>(format.nChannels);
            }
            audio_dev.sample_rate_hz =
                static_cast<std::uint32_t>(format.nSamplesPerSec);
        }

        devices.push_back(std::move(audio_dev));
    }

    return devices;
}

inline result<std::vector<::syscape::audio::audio_device>> devices() {
    return collect_devices();
}

inline result<std::vector<::syscape::audio::audio_device>> playback_devices() {
    const auto all = collect_devices();
    if (!all) {
        return fail(all.error());
    }
    return audio_common::filter_by_direction(*all, ::syscape::audio::audio_device_direction::playback);
}

inline result<std::vector<::syscape::audio::audio_device>> capture_devices() {
    const auto all = collect_devices();
    if (!all) {
        return fail(all.error());
    }
    return audio_common::filter_by_direction(*all, ::syscape::audio::audio_device_direction::capture);
}

inline result<::syscape::audio::audio_device> default_playback_device() {
    const auto all = collect_devices(eRender);
    if (!all) {
        return fail(all.error());
    }
    return audio_common::find_default_device(*all, ::syscape::audio::audio_device_direction::playback);
}

inline result<::syscape::audio::audio_device> default_capture_device() {
    const auto all = collect_devices(eCapture);
    if (!all) {
        return fail(all.error());
    }
    return audio_common::find_default_device(*all, ::syscape::audio::audio_device_direction::capture);
}

inline result<std::size_t> device_count() {
    const auto all = collect_devices();
    if (!all) {
        return fail(all.error());
    }
    return all->size();
}

} // namespace audio_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_AUDIO_WINDOWS_HPP
