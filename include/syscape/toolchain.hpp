#ifndef SYSCAPE_TOOLCHAIN_HPP
#define SYSCAPE_TOOLCHAIN_HPP

/// @file
/// @brief Allocation-free compiler and standard-library identification.
/// @note Minimum compatibility profile: Freestanding Minimal.
/// @note Minimum language version: C++11; no hosted library is required.

#include <cstddef>

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

/// Returns the compiler frontend selected for this translation unit.
constexpr compiler target_compiler() noexcept {
#if defined(SYSCAPE_FORCE_UNKNOWN_TARGET)
    return compiler::unknown;
#elif defined(__EMSCRIPTEN__)
    return compiler::emscripten;
#elif defined(__ibmxl__) || defined(__open_xl__)
    return compiler::ibm_open_xl;
#elif defined(__IBMCPP__) || defined(__xlC__)
    return compiler::ibm_xl;
#elif defined(__INTEL_LLVM_COMPILER)
    return compiler::intel_llvm;
#elif defined(__INTEL_COMPILER)
    return compiler::intel_classic;
#elif defined(__apple_build_version__) && defined(__clang__)
    return compiler::apple_clang;
#elif defined(__ARMCOMPILER_VERSION)
    return compiler::arm_compiler;
#elif defined(__TI_COMPILER_VERSION__)
    return compiler::texas_instruments;
#elif defined(__RENESAS__)
    return compiler::renesas;
#elif defined(__XC) || defined(__XC8) || defined(__XC16) || defined(__XC32)
    return compiler::microchip_xc;
#elif defined(__clang__)
    return compiler::clang;
#elif defined(_MSC_VER)
    return compiler::msvc;
#elif defined(__GNUC__)
    return compiler::gcc;
#elif defined(__SUNPRO_CC)
    return compiler::oracle_developer_studio;
#elif defined(__HP_aCC)
    return compiler::hp_acc;
#elif defined(__IAR_SYSTEMS_ICC__)
    return compiler::iar;
#elif defined(__ghs__)
    return compiler::green_hills;
#elif defined(__WATCOMC__)
    return compiler::open_watcom;
#else
    return compiler::unknown;
#endif
}

/// Returns the compiler version when exposed as numeric predefined macros.
constexpr toolchain_version target_compiler_version() noexcept {
#if defined(SYSCAPE_FORCE_UNKNOWN_TARGET)
    return {0U, 0U, 0U};
#elif defined(__EMSCRIPTEN_major__)
    return {static_cast<unsigned int>(__EMSCRIPTEN_major__),
            static_cast<unsigned int>(__EMSCRIPTEN_minor__),
            static_cast<unsigned int>(__EMSCRIPTEN_tiny__)};
#elif defined(__clang_major__)
    return {static_cast<unsigned int>(__clang_major__),
            static_cast<unsigned int>(__clang_minor__),
            static_cast<unsigned int>(__clang_patchlevel__)};
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
            static_cast<unsigned int>(__GNUC_MINOR__),
            static_cast<unsigned int>(__GNUC_PATCHLEVEL__)};
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
