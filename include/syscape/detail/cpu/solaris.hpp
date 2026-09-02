#ifndef SYSCAPE_DETAIL_CPU_SOLARIS_HPP
#define SYSCAPE_DETAIL_CPU_SOLARIS_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <set>
#include <string>
#include <sys/systeminfo.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#if defined(__has_include)
#if __has_include(<kstat.h>)
#include <kstat.h>
#define SYSCAPE_HAS_KSTAT 1
#endif
#if __has_include(<sys/auxv.h>)
#include <sys/auxv.h>
#define SYSCAPE_HAS_AUXV 1
#endif
#if __has_include(<sys/sysinfo.h>)
#include <sys/sysinfo.h>
#define SYSCAPE_HAS_SYSINFO 1
#endif
#endif

#include <syscape/detail/cpu/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace cpu_backend {

inline bool safe_add_u64(std::uint64_t a, std::uint64_t b,
                         std::uint64_t& out) noexcept {
    if (a > (std::numeric_limits<std::uint64_t>::max)() - b) {
        return false;
    }
    out = a + b;
    return true;
}

inline result<std::uint32_t> online_logical_processor_count() {
    const long count = ::sysconf(_SC_NPROCESSORS_ONLN);
    if (count <= 0) {
        return fail(errc::malformed_data);
    }
    if (static_cast<unsigned long>(count) >
        (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(count);
}

#if defined(SYSCAPE_HAS_KSTAT)

struct kstat_handle {
    ::kstat_ctl_t* ctl = nullptr;
    int error = 0;

    kstat_handle() {
        errno = 0;
        ctl = ::kstat_open();
        if (ctl == nullptr) {
            error = errno;
        }
    }
    ~kstat_handle() {
        if (ctl != nullptr) {
            ::kstat_close(ctl);
        }
    }
    kstat_handle(const kstat_handle&) = delete;
    kstat_handle& operator=(const kstat_handle&) = delete;
};

inline const char* get_kstat_string(::kstat_t* ksp, const char* name) {
    if (ksp == nullptr || name == nullptr) {
        return nullptr;
    }
    ::kstat_named_t* knp = static_cast<::kstat_named_t*>(
        ::kstat_data_lookup(ksp, const_cast<char*>(name)));
    if (knp == nullptr) {
        return nullptr;
    }
    if (knp->data_type == KSTAT_DATA_CHAR) {
        return knp->value.c;
    }
#if defined(KSTAT_DATA_STRING)
    if (knp->data_type == KSTAT_DATA_STRING) {
        return KSTAT_NAMED_STR_PTR(knp);
    }
#endif
    return nullptr;
}

inline std::int64_t get_kstat_int64(::kstat_t* ksp, const char* name,
                                    std::int64_t default_val = -1) {
    if (ksp == nullptr || name == nullptr) {
        return default_val;
    }
    ::kstat_named_t* knp = static_cast<::kstat_named_t*>(
        ::kstat_data_lookup(ksp, const_cast<char*>(name)));
    if (knp == nullptr) {
        return default_val;
    }
    switch (knp->data_type) {
    case KSTAT_DATA_INT32:
        return knp->value.i32;
    case KSTAT_DATA_UINT32:
        return knp->value.ui32;
    case KSTAT_DATA_INT64:
        return knp->value.i64;
    case KSTAT_DATA_UINT64:
        if (knp->value.ui64 > static_cast<std::uint64_t>(
                                  (std::numeric_limits<std::int64_t>::max)())) {
            return default_val;
        }
        return static_cast<std::int64_t>(knp->value.ui64);
    default:
        return default_val;
    }
}

inline std::uint64_t get_kstat_uint64(::kstat_t* ksp, const char* name,
                                      std::uint64_t default_val = 0U) {
    if (ksp == nullptr || name == nullptr) {
        return default_val;
    }
    ::kstat_named_t* knp = static_cast<::kstat_named_t*>(
        ::kstat_data_lookup(ksp, const_cast<char*>(name)));
    if (knp == nullptr) {
        return default_val;
    }
    switch (knp->data_type) {
    case KSTAT_DATA_INT32:
        return knp->value.i32 >= 0 ? static_cast<std::uint64_t>(knp->value.i32)
                                   : default_val;
    case KSTAT_DATA_UINT32:
        return knp->value.ui32;
    case KSTAT_DATA_INT64:
        return knp->value.i64 >= 0 ? static_cast<std::uint64_t>(knp->value.i64)
                                   : default_val;
    case KSTAT_DATA_UINT64:
        return knp->value.ui64;
    default:
        return default_val;
    }
}

#endif // SYSCAPE_HAS_KSTAT

inline result<std::vector<std::string>> vendor_identifiers() {
#if defined(SYSCAPE_HAS_KSTAT)
    kstat_handle k;
    if (k.ctl == nullptr) {
        if (k.error != 0) {
            return fail(std::error_code(k.error, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    std::vector<std::string> vendors;
    for (::kstat_t* ksp = k.ctl->kc_chain; ksp != nullptr; ksp = ksp->ks_next) {
        if (std::strcmp(ksp->ks_module, "cpu_info") == 0) {
            errno = 0;
            if (::kstat_read(k.ctl, ksp, nullptr) < 0) {
                if (errno != 0 && errno != ENOENT && errno != ENXIO) {
                    return fail(
                        std::error_code(errno, std::generic_category()));
                }
                continue;
            }
            const char* vendor = get_kstat_string(ksp, "vendor_id");
            if (vendor != nullptr && vendor[0] != '\0') {
                std::string v(vendor);
                if (std::find(vendors.begin(), vendors.end(), v) ==
                    vendors.end()) {
                    vendors.push_back(std::move(v));
                }
            }
        }
    }
    if (!vendors.empty()) {
        return vendors;
    }
    return fail(errc::not_supported);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::vector<std::string>> model_names() {
#if defined(SYSCAPE_HAS_KSTAT)
    kstat_handle k;
    if (k.ctl == nullptr) {
        if (k.error != 0) {
            return fail(std::error_code(k.error, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    std::vector<std::string> models;
    for (::kstat_t* ksp = k.ctl->kc_chain; ksp != nullptr; ksp = ksp->ks_next) {
        if (std::strcmp(ksp->ks_module, "cpu_info") == 0) {
            errno = 0;
            if (::kstat_read(k.ctl, ksp, nullptr) < 0) {
                if (errno != 0 && errno != ENOENT && errno != ENXIO) {
                    return fail(
                        std::error_code(errno, std::generic_category()));
                }
                continue;
            }
            const char* brand = get_kstat_string(ksp, "brand");
            if (brand == nullptr || brand[0] == '\0') {
                brand = get_kstat_string(ksp, "cpu_type");
            }
            if (brand != nullptr && brand[0] != '\0') {
                std::string m(brand);
                if (std::find(models.begin(), models.end(), m) ==
                    models.end()) {
                    models.push_back(std::move(m));
                }
            }
        }
    }
    if (!models.empty()) {
        return models;
    }
    return fail(errc::not_supported);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint32_t> online_physical_core_count() {
#if defined(SYSCAPE_HAS_KSTAT)
    kstat_handle k;
    if (k.ctl == nullptr) {
        if (k.error != 0) {
            return fail(std::error_code(k.error, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    std::set<std::int64_t> core_ids;
    for (::kstat_t* ksp = k.ctl->kc_chain; ksp != nullptr; ksp = ksp->ks_next) {
        if (std::strcmp(ksp->ks_module, "cpu_info") == 0) {
            errno = 0;
            if (::kstat_read(k.ctl, ksp, nullptr) < 0) {
                if (errno != 0 && errno != ENOENT && errno != ENXIO) {
                    return fail(
                        std::error_code(errno, std::generic_category()));
                }
                continue;
            }
            const char* state = get_kstat_string(ksp, "state");
            if (state != nullptr && std::strcmp(state, "on-line") == 0) {
                std::int64_t core_id = get_kstat_int64(ksp, "core_id");
                if (core_id >= 0) {
                    core_ids.insert(core_id);
                }
            }
        }
    }
    if (!core_ids.empty()) {
        if (core_ids.size() > (std::numeric_limits<std::uint32_t>::max)()) {
            return fail(errc::value_too_large);
        }
        return static_cast<std::uint32_t>(core_ids.size());
    }
    return fail(errc::not_supported);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint32_t> online_processor_package_count() {
#if defined(SYSCAPE_HAS_KSTAT)
    kstat_handle k;
    if (k.ctl == nullptr) {
        if (k.error != 0) {
            return fail(std::error_code(k.error, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    std::set<std::int64_t> chip_ids;
    for (::kstat_t* ksp = k.ctl->kc_chain; ksp != nullptr; ksp = ksp->ks_next) {
        if (std::strcmp(ksp->ks_module, "cpu_info") == 0) {
            errno = 0;
            if (::kstat_read(k.ctl, ksp, nullptr) < 0) {
                if (errno != 0 && errno != ENOENT && errno != ENXIO) {
                    return fail(
                        std::error_code(errno, std::generic_category()));
                }
                continue;
            }
            const char* state = get_kstat_string(ksp, "state");
            if (state != nullptr && std::strcmp(state, "on-line") == 0) {
                std::int64_t chip_id = get_kstat_int64(ksp, "chip_id");
                if (chip_id < 0) {
                    chip_id = get_kstat_int64(ksp, "pkg_core_id");
                }
                if (chip_id >= 0) {
                    chip_ids.insert(chip_id);
                }
            }
        }
    }
    if (!chip_ids.empty()) {
        if (chip_ids.size() > (std::numeric_limits<std::uint32_t>::max)()) {
            return fail(errc::value_too_large);
        }
        return static_cast<std::uint32_t>(chip_ids.size());
    }
    return fail(errc::not_supported);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::vector<std::uint32_t>> current_frequencies_khz() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> minimum_frequency_khz() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> maximum_frequency_khz() {
    return fail(errc::not_supported);
}

inline result<std::vector<cpu_common::cache_entry>> cache_descriptors() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> instruction_set_features() {
    std::vector<std::string> features;
#if defined(SYSCAPE_HAS_AUXV) && defined(AT_SUN_CAP_HW1)
    uint_t flags[4] = {};
    const uint_t n = ::getisax(flags, 4);
    if (n > 0) {
        // Solaris ISA features for x86 or SPARC
#if defined(AV_386_FPU)
        if (flags[0] & AV_386_FPU)
            features.push_back("fpu");
#endif
#if defined(AV_386_TSC)
        if (flags[0] & AV_386_TSC)
            features.push_back("tsc");
#endif
#if defined(AV_386_CX8)
        if (flags[0] & AV_386_CX8)
            features.push_back("cx8");
#endif
#if defined(AV_386_MMX)
        if (flags[0] & AV_386_MMX)
            features.push_back("mmx");
#endif
#if defined(AV_386_FXSR)
        if (flags[0] & AV_386_FXSR)
            features.push_back("fxsr");
#endif
#if defined(AV_386_SSE)
        if (flags[0] & AV_386_SSE)
            features.push_back("sse");
#endif
#if defined(AV_386_SSE2)
        if (flags[0] & AV_386_SSE2)
            features.push_back("sse2");
#endif
#if defined(AV_386_SSE3)
        if (flags[0] & AV_386_SSE3)
            features.push_back("sse3");
#endif
#if defined(AV_386_SSSE3)
        if (flags[0] & AV_386_SSSE3)
            features.push_back("ssse3");
#endif
#if defined(AV_386_SSE4_1)
        if (flags[0] & AV_386_SSE4_1)
            features.push_back("sse4_1");
#endif
#if defined(AV_386_SSE4_2)
        if (flags[0] & AV_386_SSE4_2)
            features.push_back("sse4_2");
#endif
#if defined(AV_386_AVX)
        if (flags[0] & AV_386_AVX)
            features.push_back("avx");
#endif
#if defined(AV_386_AVX2)
        if (flags[0] & AV_386_AVX2)
            features.push_back("avx2");
#endif
#if defined(AV_386_AES)
        if (flags[0] & AV_386_AES)
            features.push_back("aes");
#endif
#if defined(AV_386_PCLMULQDQ)
        if (flags[0] & AV_386_PCLMULQDQ)
            features.push_back("pclmulqdq");
#endif
    }
#endif
    if (!features.empty()) {
        return features;
    }
    return fail(errc::not_supported);
}

inline result<cpu_common::usage_information> cumulative_processor_usage() {
#if defined(SYSCAPE_HAS_KSTAT)
    kstat_handle k;
    if (k.ctl == nullptr) {
        if (k.error != 0) {
            return fail(std::error_code(k.error, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    std::uint64_t user_ticks = 0U;
    std::uint64_t system_ticks = 0U;
    std::uint64_t idle_ticks = 0U;
    std::uint64_t wait_ticks = 0U;
    bool found = false;

    for (::kstat_t* ksp = k.ctl->kc_chain; ksp != nullptr; ksp = ksp->ks_next) {
        if (std::strcmp(ksp->ks_module, "cpu_stat") == 0) {
            errno = 0;
            if (::kstat_read(k.ctl, ksp, nullptr) < 0) {
                if (errno != 0 && errno != ENOENT && errno != ENXIO) {
                    return fail(
                        std::error_code(errno, std::generic_category()));
                }
                continue;
            }
#if defined(KSTAT_TYPE_RAW) && defined(SYSCAPE_HAS_SYSINFO) &&                 \
    defined(CPU_USER) && defined(CPU_KERNEL) && defined(CPU_IDLE) &&           \
    defined(CPU_WAIT)
            if (ksp->ks_type == KSTAT_TYPE_RAW && ksp->ks_data != nullptr &&
                ksp->ks_data_size >= sizeof(::cpu_stat_t)) {
                const auto* cs = static_cast<const ::cpu_stat_t*>(ksp->ks_data);
                const auto u =
                    static_cast<std::uint64_t>(cs->cpu_sysinfo.cpu[CPU_USER]);
                const auto s =
                    static_cast<std::uint64_t>(cs->cpu_sysinfo.cpu[CPU_KERNEL]);
                const auto i =
                    static_cast<std::uint64_t>(cs->cpu_sysinfo.cpu[CPU_IDLE]);
                const auto w =
                    static_cast<std::uint64_t>(cs->cpu_sysinfo.cpu[CPU_WAIT]);
                if (!safe_add_u64(user_ticks, u, user_ticks) ||
                    !safe_add_u64(system_ticks, s, system_ticks) ||
                    !safe_add_u64(idle_ticks, i, idle_ticks) ||
                    !safe_add_u64(wait_ticks, w, wait_ticks)) {
                    return fail(errc::value_too_large);
                }
                found = true;
                continue;
            }
#endif
#if defined(KSTAT_TYPE_NAMED)
            if (ksp->ks_type == KSTAT_TYPE_NAMED) {
                const auto* kn =
                    static_cast<const ::kstat_named_t*>(::kstat_data_lookup(
                        ksp, const_cast<char*>("cpu_ticks_user")));
                if (kn != nullptr) {
                    std::int64_t u = get_kstat_int64(ksp, "cpu_ticks_user", -1);
                    std::int64_t s =
                        get_kstat_int64(ksp, "cpu_ticks_kernel", -1);
                    std::int64_t i = get_kstat_int64(ksp, "cpu_ticks_idle", -1);
                    std::int64_t w = get_kstat_int64(ksp, "cpu_ticks_wait", -1);
                    if (u >= 0 && s >= 0 && i >= 0 && w >= 0) {
                        if (!safe_add_u64(user_ticks,
                                          static_cast<std::uint64_t>(u),
                                          user_ticks) ||
                            !safe_add_u64(system_ticks,
                                          static_cast<std::uint64_t>(s),
                                          system_ticks) ||
                            !safe_add_u64(idle_ticks,
                                          static_cast<std::uint64_t>(i),
                                          idle_ticks) ||
                            !safe_add_u64(wait_ticks,
                                          static_cast<std::uint64_t>(w),
                                          wait_ticks)) {
                            return fail(errc::value_too_large);
                        }
                        found = true;
                    }
                }
            }
#endif
        }
    }
    if (found) {
        std::uint64_t combined_idle = 0U;
        if (!safe_add_u64(idle_ticks, wait_ticks, combined_idle)) {
            return fail(errc::value_too_large);
        }
        std::uint64_t total_ticks = 0U;
        if (!safe_add_u64(user_ticks, system_ticks, total_ticks) ||
            !safe_add_u64(total_ticks, combined_idle, total_ticks)) {
            return fail(errc::value_too_large);
        }
        if (total_ticks == 0U) {
            return fail(errc::malformed_data);
        }
        cpu_common::usage_information usage {};
        usage.user_ticks = user_ticks;
        usage.system_ticks = system_ticks;
        usage.idle_ticks = combined_idle;
        return usage;
    }
    return fail(errc::not_supported);
#else
    return fail(errc::not_supported);
#endif
}

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#endif
