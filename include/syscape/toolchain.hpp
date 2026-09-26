#ifndef SYSCAPE_TOOLCHAIN_HPP
#define SYSCAPE_TOOLCHAIN_HPP

/// @file
/// @brief Allocation-free compiler and standard-library identification.
/// @note Minimum compatibility profile: Freestanding Minimal.
/// @note Minimum language version: C++11; no hosted library is required.

#if defined(__has_include)
#if __has_include(<cstddef>)
#include <cstddef>
#elif __has_include(<stddef.h>)
#include <stddef.h>
namespace std {
using ::ptrdiff_t;
using ::size_t;
} // namespace std
#endif
#else
#include <cstddef>
#endif

#include <syscape/detail/config.hpp>

namespace syscape {

/// Identifies the compiler frontend used for the current translation unit.
enum class compiler {
    unknown,
    gcc,
    clang,
    apple_clang,
    msvc,
    intel_classic,
    intel_llvm,
    ibm_xl,
    ibm_open_xl,
    oracle_developer_studio,
    hp_acc,
    iar,
    arm_compiler,
    green_hills,
    texas_instruments,
    renesas,
    microchip_xc,
    open_watcom,
    emscripten
};

/// Identifies a C++ standard-library implementation visible to this header.
enum class standard_library {
    unknown,
    libstdcxx,
    libcxx,
    msvc_stl,
    dinkumware
};

/// Represents a three-component tool version.
struct toolchain_version {
    unsigned int major;
    unsigned int minor;
    unsigned int patch;
};

/// Compares two toolchain_version values for equality.
constexpr bool operator==(const toolchain_version& lhs,
                          const toolchain_version& rhs) noexcept {
    return lhs.major == rhs.major && lhs.minor == rhs.minor &&
           lhs.patch == rhs.patch;
}

/// Compares two toolchain_version values for inequality.
constexpr bool operator!=(const toolchain_version& lhs,
                          const toolchain_version& rhs) noexcept {
    return !(lhs == rhs);
}

/// Returns the compiler frontend selected for this translation unit.
constexpr compiler target_compiler() noexcept {
#if defined(SYSCAPE_FORCE_UNKNOWN_TARGET)
    return compiler::unknown;
#elif defined(__EMSCRIPTEN__)
    return compiler::emscripten;
#elif defined(__open_xl_version__) || defined(__open_xl__) || defined(__ibmxl__)
    return compiler::ibm_open_xl;
#elif defined(__xlC__) || defined(__IBMCPP__)
    return compiler::ibm_xl;
#elif defined(__INTEL_LLVM_COMPILER)
    return compiler::intel_llvm;
#elif defined(__INTEL_COMPILER)
    return compiler::intel_classic;
#elif defined(__apple_build_version__) && defined(__clang__)
    return compiler::apple_clang;
#elif defined(__ARMCOMPILER_VERSION) || defined(__ARMCC_VERSION) ||            \
    defined(__CC_ARM)
    return compiler::arm_compiler;
#elif defined(__TI_COMPILER_VERSION__)
    return compiler::texas_instruments;
#elif defined(__RENESAS__) || defined(__RENESAS_VERSION__)
    return compiler::renesas;
#elif defined(__XC) || defined(__XC8) || defined(__XC16) || defined(__XC32) || \
    defined(__XC8_VERSION) || defined(__XC16_VERSION) ||                       \
    defined(__XC32_VERSION)
    return compiler::microchip_xc;
#elif defined(__SUNPRO_CC) || defined(__SUNPRO_C)
    return compiler::oracle_developer_studio;
#elif defined(__HP_aCC)
    return compiler::hp_acc;
#elif defined(__IAR_SYSTEMS_ICC__)
    return compiler::iar;
#elif defined(__ghs__) || defined(__GHS_VERSION_NUMBER__) ||                   \
    defined(__ghs_version__)
    return compiler::green_hills;
#elif defined(__WATCOMC__)
    return compiler::open_watcom;
#elif defined(__clang__)
    return compiler::clang;
#elif defined(_MSC_VER)
    return compiler::msvc;
#elif defined(__GNUC__)
    return compiler::gcc;
#else
    return compiler::unknown;
#endif
}

/// Returns the compiler version when exposed as numeric predefined macros.
constexpr toolchain_version target_compiler_version() noexcept {
#if defined(SYSCAPE_FORCE_UNKNOWN_TARGET)
    return {0U, 0U, 0U};
#elif defined(__EMSCRIPTEN__)
#if defined(__EMSCRIPTEN_major__)
    return {static_cast<unsigned int>(__EMSCRIPTEN_major__),
#if defined(__EMSCRIPTEN_minor__)
            static_cast<unsigned int>(__EMSCRIPTEN_minor__),
#else
            0U,
#endif
#if defined(__EMSCRIPTEN_tiny__)
            static_cast<unsigned int>(__EMSCRIPTEN_tiny__)
#else
            0U
#endif
    };
#elif defined(__clang_major__)
    return {static_cast<unsigned int>(__clang_major__),
#if defined(__clang_minor__)
            static_cast<unsigned int>(__clang_minor__),
#else
            0U,
#endif
#if defined(__clang_patchlevel__)
            static_cast<unsigned int>(__clang_patchlevel__)
#else
            0U
#endif
    };
#else
    return {0U, 0U, 0U};
#endif
#elif defined(__open_xl_version__) || defined(__open_xl__) || defined(__ibmxl__)
#if defined(__open_xl_version__)
    return {static_cast<unsigned int>(__open_xl_version__),
#if defined(__open_xl_release__)
            static_cast<unsigned int>(__open_xl_release__),
#else
            0U,
#endif
#if defined(__open_xl_modification__)
            static_cast<unsigned int>(__open_xl_modification__)
#elif defined(__open_xl_ptf__)
            static_cast<unsigned int>(__open_xl_ptf__)
#else
            0U
#endif
    };
#elif defined(__ibmxl_version__)
    return {static_cast<unsigned int>(__ibmxl_version__),
#if defined(__ibmxl_release__)
            static_cast<unsigned int>(__ibmxl_release__),
#else
            0U,
#endif
#if defined(__ibmxl_modification__)
            static_cast<unsigned int>(__ibmxl_modification__)
#elif defined(__ibmxl_ptf__)
            static_cast<unsigned int>(__ibmxl_ptf__)
#else
            0U
#endif
    };
#elif defined(__clang_major__)
    return {static_cast<unsigned int>(__clang_major__),
            static_cast<unsigned int>(__clang_minor__),
            static_cast<unsigned int>(__clang_patchlevel__)};
#else
    return {0U, 0U, 0U};
#endif
#elif defined(__xlC__) || defined(__IBMCPP__)
#if defined(__xlC__)
    return {static_cast<unsigned int>((((__xlC__ >> 12) & 0xF) * 10) +
                                      ((__xlC__ >> 8) & 0xF)),
            static_cast<unsigned int>((((__xlC__ >> 4) & 0xF) * 10) +
                                      (__xlC__ & 0xF)),
#if defined(__xlC_ver__)
            static_cast<unsigned int>((((__xlC_ver__ >> 12) & 0xF) * 10) +
                                      ((__xlC_ver__ >> 8) & 0xF))
#else
            0U
#endif
    };
#elif defined(__IBMCPP__)
    return {static_cast<unsigned int>(__IBMCPP__ / 100),
            static_cast<unsigned int>((__IBMCPP__ % 100) / 10),
            static_cast<unsigned int>(__IBMCPP__ % 10)};
#else
    return {0U, 0U, 0U};
#endif
#elif defined(__INTEL_LLVM_COMPILER)
    return {static_cast<unsigned int>(__INTEL_LLVM_COMPILER / 10000),
            static_cast<unsigned int>((__INTEL_LLVM_COMPILER % 10000) / 100),
            static_cast<unsigned int>(__INTEL_LLVM_COMPILER % 100)};
#elif defined(__INTEL_COMPILER)
    return {static_cast<unsigned int>(__INTEL_COMPILER / 100),
            static_cast<unsigned int>((__INTEL_COMPILER % 100) / 10),
#if defined(__INTEL_COMPILER_UPDATE)
            static_cast<unsigned int>(__INTEL_COMPILER_UPDATE)
#else
            static_cast<unsigned int>(__INTEL_COMPILER % 10)
#endif
    };
#elif defined(__apple_build_version__) && defined(__clang__)
#if defined(__clang_major__)
    return {static_cast<unsigned int>(__clang_major__),
#if defined(__clang_minor__)
            static_cast<unsigned int>(__clang_minor__),
#else
            0U,
#endif
#if defined(__clang_patchlevel__)
            static_cast<unsigned int>(__clang_patchlevel__)
#else
            0U
#endif
    };
#else
    return {0U, 0U, 0U};
#endif
#elif defined(__ARMCOMPILER_VERSION) || defined(__ARMCC_VERSION) ||            \
    defined(__CC_ARM)
#if defined(__ARMCOMPILER_VERSION)
    return {static_cast<unsigned int>(__ARMCOMPILER_VERSION / 1000000),
            static_cast<unsigned int>((__ARMCOMPILER_VERSION / 10000) % 100),
            static_cast<unsigned int>((__ARMCOMPILER_VERSION / 100) % 100)};
#elif defined(__ARMCC_VERSION)
    return {static_cast<unsigned int>(__ARMCC_VERSION / 1000000),
            static_cast<unsigned int>((__ARMCC_VERSION / 10000) % 100),
            static_cast<unsigned int>((__ARMCC_VERSION / 100) % 100)};
#else
    return {0U, 0U, 0U};
#endif
#elif defined(__TI_COMPILER_VERSION__)
    return {static_cast<unsigned int>(__TI_COMPILER_VERSION__ / 1000000),
            static_cast<unsigned int>((__TI_COMPILER_VERSION__ / 1000) % 1000),
            static_cast<unsigned int>(__TI_COMPILER_VERSION__ % 1000)};
#elif defined(__RENESAS__) || defined(__RENESAS_VERSION__)
#if defined(__RENESAS_VERSION__)
#if (__RENESAS_VERSION__ > 0xFFFF)
    return {static_cast<unsigned int>((__RENESAS_VERSION__ >> 24) & 0xFF),
            static_cast<unsigned int>((__RENESAS_VERSION__ >> 16) & 0xFF),
            static_cast<unsigned int>((__RENESAS_VERSION__ >> 8) & 0xFF)};
#else
    return {static_cast<unsigned int>((__RENESAS_VERSION__ >> 8) & 0xFF),
            static_cast<unsigned int>(__RENESAS_VERSION__ & 0xFF), 0U};
#endif
#else
    return {0U, 0U, 0U};
#endif
#elif defined(__XC) || defined(__XC8) || defined(__XC16) || defined(__XC32) || \
    defined(__XC8_VERSION) || defined(__XC16_VERSION) ||                       \
    defined(__XC32_VERSION)
#if defined(__XC32_VERSION)
#define SYSCAPE_DETAIL_XC_VER __XC32_VERSION
#elif defined(__XC16_VERSION)
#define SYSCAPE_DETAIL_XC_VER __XC16_VERSION
#elif defined(__XC8_VERSION)
#define SYSCAPE_DETAIL_XC_VER __XC8_VERSION
#endif
#if defined(SYSCAPE_DETAIL_XC_VER)
    return {static_cast<unsigned int>(SYSCAPE_DETAIL_XC_VER >= 1000
                                          ? SYSCAPE_DETAIL_XC_VER / 1000
                                          : SYSCAPE_DETAIL_XC_VER / 100),
            static_cast<unsigned int>(SYSCAPE_DETAIL_XC_VER >= 1000
                                          ? (SYSCAPE_DETAIL_XC_VER % 1000) / 10
                                          : SYSCAPE_DETAIL_XC_VER % 100),
            static_cast<unsigned int>(SYSCAPE_DETAIL_XC_VER >= 1000
                                          ? SYSCAPE_DETAIL_XC_VER % 10
                                          : 0U)};
#undef SYSCAPE_DETAIL_XC_VER
#else
    return {0U, 0U, 0U};
#endif
#elif defined(__SUNPRO_CC) || defined(__SUNPRO_C)
#if defined(__SUNPRO_CC)
#define SYSCAPE_DETAIL_SUNPRO_VER __SUNPRO_CC
#else
#define SYSCAPE_DETAIL_SUNPRO_VER __SUNPRO_C
#endif
    return {
        static_cast<unsigned int>(SYSCAPE_DETAIL_SUNPRO_VER >= 0x5100
                                      ? (SYSCAPE_DETAIL_SUNPRO_VER >> 12) & 0xF
                                      : (SYSCAPE_DETAIL_SUNPRO_VER >> 8) & 0xF),
        static_cast<unsigned int>(
            SYSCAPE_DETAIL_SUNPRO_VER >= 0x5100
                ? (((SYSCAPE_DETAIL_SUNPRO_VER >> 8) & 0xF) * 10) +
                      ((SYSCAPE_DETAIL_SUNPRO_VER >> 4) & 0xF)
                : (SYSCAPE_DETAIL_SUNPRO_VER >> 4) & 0xF),
        static_cast<unsigned int>(SYSCAPE_DETAIL_SUNPRO_VER & 0xF)};
#undef SYSCAPE_DETAIL_SUNPRO_VER
#elif defined(__HP_aCC)
    return {static_cast<unsigned int>(__HP_aCC / 10000),
            static_cast<unsigned int>((__HP_aCC % 10000) / 100),
            static_cast<unsigned int>(__HP_aCC % 100)};
#elif defined(__IAR_SYSTEMS_ICC__)
#if defined(__VER__)
    return {
        static_cast<unsigned int>(__VER__ >= 1000000 ? __VER__ / 1000000
                                                     : __VER__ / 100),
        static_cast<unsigned int>(__VER__ >= 1000000 ? (__VER__ / 1000) % 1000
                                                     : __VER__ % 100),
        static_cast<unsigned int>(__VER__ >= 1000000 ? __VER__ % 1000 : 0U)};
#else
    return {0U, 0U, 0U};
#endif
#elif defined(__ghs__) || defined(__GHS_VERSION_NUMBER__) ||                   \
    defined(__ghs_version__)
#if defined(__GHS_VERSION_NUMBER__)
    return {static_cast<unsigned int>(__GHS_VERSION_NUMBER__ / 100),
            static_cast<unsigned int>((__GHS_VERSION_NUMBER__ % 100) / 10),
            static_cast<unsigned int>(__GHS_VERSION_NUMBER__ % 10)};
#elif defined(__ghs_version__)
    return {static_cast<unsigned int>(__ghs_version__ / 100),
            static_cast<unsigned int>((__ghs_version__ % 100) / 10),
            static_cast<unsigned int>(__ghs_version__ % 10)};
#else
    return {0U, 0U, 0U};
#endif
#elif defined(__WATCOMC__)
    return {static_cast<unsigned int>(__WATCOMC__ >= 1200
                                          ? (__WATCOMC__ - 1100) / 100
                                          : __WATCOMC__ / 100),
            static_cast<unsigned int>(__WATCOMC__ >= 1200
                                          ? ((__WATCOMC__ - 1100) % 100) / 10
                                          : (__WATCOMC__ % 100) / 10),
            static_cast<unsigned int>(__WATCOMC__ % 10)};
#elif defined(__clang__)
#if defined(__clang_major__)
    return {static_cast<unsigned int>(__clang_major__),
#if defined(__clang_minor__)
            static_cast<unsigned int>(__clang_minor__),
#else
            0U,
#endif
#if defined(__clang_patchlevel__)
            static_cast<unsigned int>(__clang_patchlevel__)
#else
            0U
#endif
    };
#else
    return {0U, 0U, 0U};
#endif
#elif defined(_MSC_VER)
    return {static_cast<unsigned int>(_MSC_VER / 100),
            static_cast<unsigned int>(_MSC_VER % 100),
#if defined(_MSC_FULL_VER)
            static_cast<unsigned int>(_MSC_FULL_VER % 100000)
#else
            0U
#endif
    };
#elif defined(__GNUC__)
    return {static_cast<unsigned int>(__GNUC__),
#if defined(__GNUC_MINOR__)
            static_cast<unsigned int>(__GNUC_MINOR__),
#else
            0U,
#endif
#if defined(__GNUC_PATCHLEVEL__)
            static_cast<unsigned int>(__GNUC_PATCHLEVEL__)
#else
            0U
#endif
    };
#else
    return {0U, 0U, 0U};
#endif
}

/// Returns the C++ language-version value used by the compiler.
constexpr long target_cpp_version() noexcept {
    return static_cast<long>(SYSCAPE_DETAIL_CPLUSPLUS);
}

/// Returns the standard-library implementation visible to this header.
constexpr standard_library target_standard_library() noexcept {
#if defined(SYSCAPE_FORCE_UNKNOWN_TARGET)
    return standard_library::unknown;
#elif defined(_LIBCPP_VERSION)
    return standard_library::libcxx;
#elif defined(__GLIBCXX__)
    return standard_library::libstdcxx;
#elif defined(_MSVC_STL_VERSION)
    return standard_library::msvc_stl;
#elif defined(_CPPLIB_VER)
    return standard_library::dinkumware;
#else
    return standard_library::unknown;
#endif
}

/// Returns a stable English name for a compiler value.
SYSCAPE_DETAIL_CONSTEXPR14 const char* compiler_name(compiler value) noexcept {
    switch (value) {
    case compiler::gcc: return "gcc";
    case compiler::clang: return "clang";
    case compiler::apple_clang: return "apple-clang";
    case compiler::msvc: return "msvc";
    case compiler::intel_classic: return "intel-classic";
    case compiler::intel_llvm: return "intel-llvm";
    case compiler::ibm_xl: return "ibm-xl";
    case compiler::ibm_open_xl: return "ibm-open-xl";
    case compiler::oracle_developer_studio: return "oracle-developer-studio";
    case compiler::hp_acc: return "hp-acc";
    case compiler::iar: return "iar";
    case compiler::arm_compiler: return "arm-compiler";
    case compiler::green_hills: return "green-hills";
    case compiler::texas_instruments: return "texas-instruments";
    case compiler::renesas: return "renesas";
    case compiler::microchip_xc: return "microchip-xc";
    case compiler::open_watcom: return "open-watcom";
    case compiler::emscripten: return "emscripten";
    case compiler::unknown: return "unknown";
    }
    return "unknown";
}

/// Returns a stable English name for a standard-library value.
SYSCAPE_DETAIL_CONSTEXPR14 const char* standard_library_name(
    standard_library value) noexcept {
    switch (value) {
    case standard_library::libstdcxx: return "libstdc++";
    case standard_library::libcxx: return "libc++";
    case standard_library::msvc_stl: return "msvc-stl";
    case standard_library::dinkumware: return "dinkumware";
    case standard_library::unknown: return "unknown";
    }
    return "unknown";
}

} // namespace syscape

#endif
