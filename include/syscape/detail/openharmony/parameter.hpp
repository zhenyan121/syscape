#ifndef SYSCAPE_DETAIL_OPENHARMONY_PARAMETER_HPP
#define SYSCAPE_DETAIL_OPENHARMONY_PARAMETER_HPP

#include <cstddef>
#include <string>
#include <system_error>
#include <vector>

#if defined(__OHOS__) || defined(__OpenHarmony__)
#include <dlfcn.h>
#endif

#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace openharmony {

constexpr std::size_t maximum_constant_parameter_length = 4096U;

#if defined(__OHOS__) || defined(__OpenHarmony__)
namespace beget_detail {

using SystemReadParamFn = int (*)(const char*, char*, unsigned int*);
using GetParameterFn = int (*)(const char*, const char*, char*, unsigned int);
using GetStringFn = const char* (*)(void);
using GetIntFn = int (*)(void);

inline errc map_param_error(int error_code) noexcept {
    switch (error_code) {
    case 116:       // PARAM_CODE_PERMISSION_DENIED
    case 1001:      // DAC_RESULT_FORBIDED
    case 1002:      // SELINUX_RESULT_FORBIDED
    case -14700103: // SYSPARAM_PERMISSION_DENIED
        return errc::permission_denied;
    case 100:       // PARAM_CODE_INVALID_PARAM
    case 101:       // PARAM_CODE_INVALID_NAME
    case 102:       // PARAM_CODE_INVALID_VALUE
    case 107:       // PARAM_CODE_READ_ONLY
    case 109:       // PARAM_CODE_NODE_EXIST
    case 110:       // PARAM_WATCHER_CALLBACK_EXIST
    case 1000:      // DAC_RESULT_INVALID_PARAM
    case -9:        // EC_INVALID
    case -401:      // SYSPARAM_INVALID_INPUT
    case -14700102: // SYSPARAM_INVALID_VALUE
        return errc::invalid_argument;
    case 104: // PARAM_CODE_NOT_SUPPORT
        return errc::not_supported;
    case 105:       // PARAM_CODE_TIMEOUT
    case 108:       // PARAM_CODE_IPC_ERROR
    case 111:       // PARAM_WATCHER_GET_SERVICE_FAILED
    case 113:       // PARAM_WORKSPACE_NOT_INIT
    case 114:       // PARAM_CODE_FAIL_CONNECT
    case -14700105: // SYSPARAM_WAIT_TIMEOUT
        return errc::temporarily_unavailable;
    case 103: // PARAM_CODE_REACHED_MAX
    case 112: // PARAM_CODE_MEMORY_MAP_FAILED
    case 115: // PARAM_CODE_MEMORY_NOT_ENOUGH
    case 117: // PARAM_DEFAULT_PARAM_MEMORY_NOT_ENOUGH
        return errc::resource_exhausted;
    case 106:       // PARAM_CODE_NOT_FOUND
    case -14700101: // SYSPARAM_NOT_FOUND
        return errc::not_found;
    case -1: // PARAM_CODE_ERROR / EC_FAILURE (system or initialization failure)
    default:
        return errc::io_error;
    }
}

class module_handle {
    public:
    module_handle() noexcept {
        handle_ = ::dlopen("libbegetutil.z.so", RTLD_LAZY | RTLD_LOCAL);
        if (handle_ == nullptr) {
            handle_ = ::dlopen("libbegetutil.so", RTLD_LAZY | RTLD_LOCAL);
        }
        if (handle_ == nullptr) {
            handle_ = ::dlopen(nullptr, RTLD_LAZY | RTLD_LOCAL);
        }
    }

    ~module_handle() {
        if (handle_ != nullptr) {
            ::dlclose(handle_);
        }
    }

    module_handle(const module_handle&) = delete;
    module_handle& operator=(const module_handle&) = delete;

    void* get() const noexcept {
        return handle_;
    }

    private:
    void* handle_ {nullptr};
};

struct api_table {
    module_handle module {};
    SystemReadParamFn system_read_param {nullptr};
    GetParameterFn get_parameter {nullptr};
    GetStringFn get_distribution_os_name {nullptr};
    GetStringFn get_distribution_os_version {nullptr};
    GetStringFn get_incremental_version {nullptr};
    GetStringFn get_os_full_name {nullptr};
    GetStringFn get_display_version {nullptr};
    GetIntFn get_sdk_api_version {nullptr};
    GetStringFn get_device_type {nullptr};
    GetStringFn get_manufacture {nullptr};
    GetStringFn get_product_model {nullptr};
    GetStringFn get_brand {nullptr};
    GetStringFn get_security_patch_tag {nullptr};

    api_table() noexcept {
        void* handle = module.get();
        if (handle != nullptr) {
            system_read_param = reinterpret_cast<SystemReadParamFn>(
                ::dlsym(handle, "SystemReadParam"));
            get_parameter = reinterpret_cast<GetParameterFn>(
                ::dlsym(handle, "GetParameter"));
            get_distribution_os_name = reinterpret_cast<GetStringFn>(
                ::dlsym(handle, "GetDistributionOSName"));
            get_distribution_os_version = reinterpret_cast<GetStringFn>(
                ::dlsym(handle, "GetDistributionOSVersion"));
            get_incremental_version = reinterpret_cast<GetStringFn>(
                ::dlsym(handle, "GetIncrementalVersion"));
            get_os_full_name =
                reinterpret_cast<GetStringFn>(::dlsym(handle, "GetOSFullName"));
            get_display_version = reinterpret_cast<GetStringFn>(
                ::dlsym(handle, "GetDisplayVersion"));
            get_sdk_api_version =
                reinterpret_cast<GetIntFn>(::dlsym(handle, "GetSdkApiVersion"));
            get_device_type =
                reinterpret_cast<GetStringFn>(::dlsym(handle, "GetDeviceType"));
            get_manufacture = reinterpret_cast<GetStringFn>(
                ::dlsym(handle, "GetManufacture"));
            get_product_model = reinterpret_cast<GetStringFn>(
                ::dlsym(handle, "GetProductModel"));
            get_brand =
                reinterpret_cast<GetStringFn>(::dlsym(handle, "GetBrand"));
            get_security_patch_tag = reinterpret_cast<GetStringFn>(
                ::dlsym(handle, "GetSecurityPatchTag"));
        }
    }

    ~api_table() = default;
    api_table(const api_table&) = delete;
    api_table& operator=(const api_table&) = delete;

    static const api_table& instance() {
        static const api_table table;
        return table;
    }
};

} // namespace beget_detail
#endif

inline result<std::string> get_parameter(const char* key) {
#if defined(__OHOS__) || defined(__OpenHarmony__)
    if (key == nullptr || *key == '\0') {
        return fail(errc::invalid_argument);
    }
    const auto& api = beget_detail::api_table::instance();
    if (api.system_read_param != nullptr) {
        unsigned int len = 0;
        int rc = api.system_read_param(key, nullptr, &len);
        if (rc == 0) {
            if (len == 0) {
                return std::string();
            }
            constexpr unsigned int maximum_allowed_length = 64U * 1024U;
            if (len > maximum_allowed_length) {
                return fail(errc::value_too_large);
            }

            for (int attempt = 0; attempt < 3; ++attempt) {
                std::vector<char> buffer(static_cast<std::size_t>(len) + 1U,
                                         '\0');
                unsigned int actual_len = len;
                rc = api.system_read_param(key, buffer.data(), &actual_len);
                if (rc == 0) {
                    buffer[buffer.size() - 1U] = '\0';
                    return std::string(buffer.data());
                }
                if (actual_len > len && actual_len <= maximum_allowed_length) {
                    len = actual_len;
                    continue;
                }
                return fail(beget_detail::map_param_error(rc));
            }
            return fail(beget_detail::map_param_error(rc));
        } else if (rc != 100 && rc != -9 && rc != -401) {
            return fail(beget_detail::map_param_error(rc));
        }

        std::vector<char> buffer(maximum_constant_parameter_length + 1U, '\0');
        unsigned int actual_len =
            static_cast<unsigned int>(maximum_constant_parameter_length);
        rc = api.system_read_param(key, buffer.data(), &actual_len);
        if (rc == 0) {
            buffer[buffer.size() - 1U] = '\0';
            return std::string(buffer.data());
        }
        return fail(beget_detail::map_param_error(rc));
    }
    if (api.get_parameter != nullptr) {
        std::vector<char> buffer(maximum_constant_parameter_length + 1U, '\0');
        const int rc = api.get_parameter(
            key, nullptr, buffer.data(),
            static_cast<unsigned int>(maximum_constant_parameter_length));
        if (rc >= 0) {
            buffer[maximum_constant_parameter_length] = '\0';
            return std::string(buffer.data());
        }
        return fail(beget_detail::map_param_error(rc));
    }
    return fail(errc::not_supported);
#else
    static_cast<void>(key);
    return fail(errc::not_supported);
#endif
}

inline std::string get_parameter_or(const char* key,
                                    const std::string& fallback) {
    const auto res = get_parameter(key);
    return res ? *res : fallback;
}

inline result<std::string> distribution_os_name() {
#if defined(__OHOS__) || defined(__OpenHarmony__)
    const auto& api = beget_detail::api_table::instance();
    if (api.get_distribution_os_name != nullptr) {
        const char* name = api.get_distribution_os_name();
        if (name != nullptr && *name != '\0') {
            return std::string(name);
        }
    }
    return get_parameter("const.product.os.dist.name");
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> distribution_os_version() {
#if defined(__OHOS__) || defined(__OpenHarmony__)
    const auto& api = beget_detail::api_table::instance();
    if (api.get_distribution_os_version != nullptr) {
        const char* version = api.get_distribution_os_version();
        if (version != nullptr && *version != '\0') {
            return std::string(version);
        }
    }
    return get_parameter("const.product.os.dist.version");
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> incremental_version() {
#if defined(__OHOS__) || defined(__OpenHarmony__)
    const auto& api = beget_detail::api_table::instance();
    if (api.get_incremental_version != nullptr) {
        const char* inc = api.get_incremental_version();
        if (inc != nullptr && *inc != '\0') {
            return std::string(inc);
        }
    }
    return get_parameter("const.product.incremental.version");
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> os_full_name() {
#if defined(__OHOS__) || defined(__OpenHarmony__)
    const auto& api = beget_detail::api_table::instance();
    if (api.get_os_full_name != nullptr) {
        const char* name = api.get_os_full_name();
        if (name != nullptr && *name != '\0') {
            return std::string(name);
        }
    }
    return get_parameter("const.ohos.fullname");
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> display_version() {
#if defined(__OHOS__) || defined(__OpenHarmony__)
    const auto& api = beget_detail::api_table::instance();
    if (api.get_display_version != nullptr) {
        const char* version = api.get_display_version();
        if (version != nullptr && *version != '\0') {
            return std::string(version);
        }
    }
    return get_parameter("const.ohos.version.release");
#else
    return fail(errc::not_supported);
#endif
}

inline result<int> sdk_api_version() {
#if defined(__OHOS__) || defined(__OpenHarmony__)
    const auto& api = beget_detail::api_table::instance();
    if (api.get_sdk_api_version != nullptr) {
        const int api_ver = api.get_sdk_api_version();
        if (api_ver > 0) {
            return api_ver;
        }
    }
    return fail(errc::not_found);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> device_type() {
#if defined(__OHOS__) || defined(__OpenHarmony__)
    const auto& api = beget_detail::api_table::instance();
    if (api.get_device_type != nullptr) {
        const char* type = api.get_device_type();
        if (type != nullptr && *type != '\0') {
            return std::string(type);
        }
    }
    return get_parameter("const.build.characteristics");
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> manufacture() {
#if defined(__OHOS__) || defined(__OpenHarmony__)
    const auto& api = beget_detail::api_table::instance();
    if (api.get_manufacture != nullptr) {
        const char* mfg = api.get_manufacture();
        if (mfg != nullptr && *mfg != '\0') {
            return std::string(mfg);
        }
    }
    return get_parameter("const.product.manufacturer");
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> product_model() {
#if defined(__OHOS__) || defined(__OpenHarmony__)
    const auto& api = beget_detail::api_table::instance();
    if (api.get_product_model != nullptr) {
        const char* model = api.get_product_model();
        if (model != nullptr && *model != '\0') {
            return std::string(model);
        }
    }
    return get_parameter("const.product.model");
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> brand() {
#if defined(__OHOS__) || defined(__OpenHarmony__)
    const auto& api = beget_detail::api_table::instance();
    if (api.get_brand != nullptr) {
        const char* b = api.get_brand();
        if (b != nullptr && *b != '\0') {
            return std::string(b);
        }
    }
    return get_parameter("const.product.brand");
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> security_patch_tag() {
#if defined(__OHOS__) || defined(__OpenHarmony__)
    const auto& api = beget_detail::api_table::instance();
    if (api.get_security_patch_tag != nullptr) {
        const char* tag = api.get_security_patch_tag();
        if (tag != nullptr && *tag != '\0') {
            return std::string(tag);
        }
    }
    return get_parameter("const.ohos.version.security_patch");
#else
    return fail(errc::not_supported);
#endif
}

} // namespace openharmony
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_OPENHARMONY_PARAMETER_HPP
