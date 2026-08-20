#ifndef SYSCAPE_ARCHITECTURE_HPP
#define SYSCAPE_ARCHITECTURE_HPP

/// @file
/// @brief Allocation-free compile-target architecture information.
/// @note Minimum compatibility profile: Freestanding Minimal.

#include <climits>

#include <syscape/detail/config.hpp>

namespace syscape {

/// Identifies the architecture for which the current translation unit is built.
enum class architecture {
    unknown,
    x86,
    x86_64,
    arm,
    arm64,
    riscv32,
    riscv64,
    mips,
    mips64,
    powerpc,
    powerpc64,
    sparc,
    sparc64,
    s390,
    s390x,
    loongarch32,
    loongarch64,
    ia64,
    alpha,
    m68k,
    avr,
    msp430,
    xtensa,
    arc,
    csky,
    microblaze,
    nios2,
    superh,
    v850,
    rl78,
    rx,
    h8300,
    blackfin,
    cris,
    pa_risc,
    pdp11,
    vax,
    m32c,
    m32r,
    fr30,
    frv,
    pru,
    mmix,
    stormy16,
    visium,
    epiphany,
    iq2000,
    lm32,
    mep,
    mcore,
    mn10300,
    moxie,
    nds32
};

/// Identifies the target byte order when the toolchain exposes it.
enum class byte_order {
    unknown,
    little_endian,
    big_endian,
    mixed_endian
};

/// Identifies a commonly named C and C++ fundamental-type data model.
enum class data_model {
    unknown,
    lp32,
    ilp32,
    lp64,
    llp64,
    ilp64,
    other
};

/// Describes fundamental-type widths in bits for the compilation target.
struct data_model_info {
    unsigned int short_bits;
    unsigned int int_bits;
    unsigned int long_bits;
    unsigned int long_long_bits;
    unsigned int pointer_bits;
};

/// Returns the architecture selected by the compiler for this translation unit.
constexpr architecture target_architecture() noexcept {
#if defined(SYSCAPE_FORCE_UNKNOWN_TARGET)
    return architecture::unknown;
#elif defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__) || defined(__amd64__)
    return architecture::x86_64;
#elif defined(_M_IX86) || defined(__i386__)
    return architecture::x86;
#elif defined(_M_ARM64) || defined(__aarch64__)
    return architecture::arm64;
#elif defined(_M_ARM) || defined(__arm__)
    return architecture::arm;
#elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
    return architecture::riscv64;
#elif defined(__riscv)
    return architecture::riscv32;
#elif defined(__mips64) || (defined(__mips__) && defined(_MIPS_SZPTR) && (_MIPS_SZPTR == 64))
    return architecture::mips64;
#elif defined(__mips__)
    return architecture::mips;
#elif defined(__powerpc64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
    return architecture::powerpc64;
#elif defined(__powerpc__) || defined(__ppc__) || defined(_M_PPC) || defined(_ARCH_PPC)
    return architecture::powerpc;
#elif defined(__sparc__) && (defined(__arch64__) || defined(__sparcv9))
    return architecture::sparc64;
#elif defined(__sparc__)
    return architecture::sparc;
#elif defined(__s390x__)
    return architecture::s390x;
#elif defined(__s390__)
    return architecture::s390;
#elif defined(__loongarch64)
    return architecture::loongarch64;
#elif defined(__loongarch__)
    return architecture::loongarch32;
#elif defined(__ia64__) || defined(_M_IA64)
    return architecture::ia64;
#elif defined(__alpha__)
    return architecture::alpha;
#elif defined(__m68k__)
    return architecture::m68k;
#elif defined(__AVR__)
    return architecture::avr;
#elif defined(__MSP430__)
    return architecture::msp430;
#elif defined(__XTENSA__)
    return architecture::xtensa;
#elif defined(__arc__)
    return architecture::arc;
#elif defined(__csky__)
    return architecture::csky;
#elif defined(__MICROBLAZE__)
    return architecture::microblaze;
#elif defined(__nios2__)
    return architecture::nios2;
#elif defined(__sh__)
    return architecture::superh;
#elif defined(__v850__)
    return architecture::v850;
#elif defined(__RL78__)
    return architecture::rl78;
#elif defined(__RX__)
    return architecture::rx;
#elif defined(__H8300__)
    return architecture::h8300;
#elif defined(__BFIN__)
    return architecture::blackfin;
#elif defined(__cris__)
    return architecture::cris;
#elif defined(__hppa__) || defined(__HPPA__)
    return architecture::pa_risc;
#elif defined(__pdp11__)
    return architecture::pdp11;
#elif defined(__vax__)
    return architecture::vax;
#elif defined(__m32c__)
    return architecture::m32c;
#elif defined(__m32r__)
    return architecture::m32r;
#elif defined(__fr30__)
    return architecture::fr30;
#elif defined(__frv__)
    return architecture::frv;
#elif defined(__PRU__)
    return architecture::pru;
#elif defined(__mmix__)
    return architecture::mmix;
#elif defined(__mstormy16__)
    return architecture::stormy16;
#elif defined(__VISIUM__)
    return architecture::visium;
#elif defined(__epiphany__)
    return architecture::epiphany;
#elif defined(__iq2000__)
    return architecture::iq2000;
#elif defined(__lm32__)
    return architecture::lm32;
#elif defined(__mep__)
    return architecture::mep;
#elif defined(__mcore__)
    return architecture::mcore;
#elif defined(__mn10300__)
    return architecture::mn10300;
#elif defined(__moxie__)
    return architecture::moxie;
#elif defined(__nds32__)
    return architecture::nds32;
#else
    return architecture::unknown;
#endif
}

/// Returns the target byte order, or byte_order::unknown when not exposed.
constexpr byte_order target_byte_order() noexcept {
#if defined(SYSCAPE_FORCE_UNKNOWN_TARGET)
    return byte_order::unknown;
#elif defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    return byte_order::little_endian;
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return byte_order::big_endian;
#elif defined(__BYTE_ORDER__) && defined(__ORDER_PDP_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_PDP_ENDIAN__)
    return byte_order::mixed_endian;
#elif defined(_WIN32) || defined(__LITTLE_ENDIAN__) || defined(_LITTLE_ENDIAN)
    return byte_order::little_endian;
#elif defined(__BIG_ENDIAN__) || defined(_BIG_ENDIAN)
    return byte_order::big_endian;
#else
    return byte_order::unknown;
#endif
}

/// Returns the widths of fundamental types for the compilation target.
constexpr data_model_info target_data_model_info() noexcept {
    return data_model_info{
        static_cast<unsigned int>(sizeof(short) * CHAR_BIT),
        static_cast<unsigned int>(sizeof(int) * CHAR_BIT),
        static_cast<unsigned int>(sizeof(long) * CHAR_BIT),
        static_cast<unsigned int>(sizeof(long long) * CHAR_BIT),
        static_cast<unsigned int>(sizeof(void*) * CHAR_BIT)};
}

/// Returns the conventional name of the compilation target's data model.
constexpr data_model target_data_model() noexcept {
    const data_model_info value = target_data_model_info();
    if (value.int_bits == 16 && value.long_bits == 32 &&
        value.pointer_bits == 32) {
        return data_model::lp32;
    }
    if (value.int_bits == 32 && value.long_bits == 32 &&
        value.pointer_bits == 32) {
        return data_model::ilp32;
    }
    if (value.int_bits == 32 && value.long_bits == 64 &&
        value.pointer_bits == 64) {
        return data_model::lp64;
    }
    if (value.int_bits == 32 && value.long_bits == 32 &&
        value.long_long_bits == 64 && value.pointer_bits == 64) {
        return data_model::llp64;
    }
    if (value.int_bits == 64 && value.long_bits == 64 &&
        value.pointer_bits == 64) {
        return data_model::ilp64;
    }
    return data_model::other;
}

/// Returns a stable English name for an architecture value.
constexpr const char* architecture_name(architecture value) noexcept {
    switch (value) {
    case architecture::x86: return "x86";
    case architecture::x86_64: return "x86-64";
    case architecture::arm: return "arm";
    case architecture::arm64: return "arm64";
    case architecture::riscv32: return "riscv32";
    case architecture::riscv64: return "riscv64";
    case architecture::mips: return "mips";
    case architecture::mips64: return "mips64";
    case architecture::powerpc: return "powerpc";
    case architecture::powerpc64: return "powerpc64";
    case architecture::sparc: return "sparc";
    case architecture::sparc64: return "sparc64";
    case architecture::s390: return "s390";
    case architecture::s390x: return "s390x";
    case architecture::loongarch32: return "loongarch32";
    case architecture::loongarch64: return "loongarch64";
    case architecture::ia64: return "ia64";
    case architecture::alpha: return "alpha";
    case architecture::m68k: return "m68k";
    case architecture::avr: return "avr";
    case architecture::msp430: return "msp430";
    case architecture::xtensa: return "xtensa";
    case architecture::arc: return "arc";
    case architecture::csky: return "csky";
    case architecture::microblaze: return "microblaze";
    case architecture::nios2: return "nios2";
    case architecture::superh: return "superh";
    case architecture::v850: return "v850";
    case architecture::rl78: return "rl78";
    case architecture::rx: return "rx";
    case architecture::h8300: return "h8300";
    case architecture::blackfin: return "blackfin";
    case architecture::cris: return "cris";
    case architecture::pa_risc: return "pa-risc";
    case architecture::pdp11: return "pdp11";
    case architecture::vax: return "vax";
    case architecture::m32c: return "m32c";
    case architecture::m32r: return "m32r";
    case architecture::fr30: return "fr30";
    case architecture::frv: return "fr-v";
    case architecture::pru: return "pru";
    case architecture::mmix: return "mmix";
    case architecture::stormy16: return "stormy16";
    case architecture::visium: return "visium";
    case architecture::epiphany: return "epiphany";
    case architecture::iq2000: return "iq2000";
    case architecture::lm32: return "lm32";
    case architecture::mep: return "mep";
    case architecture::mcore: return "mcore";
    case architecture::mn10300: return "mn10300";
    case architecture::moxie: return "moxie";
    case architecture::nds32: return "nds32";
    case architecture::unknown: return "unknown";
    }
    return "unknown";
}

/// Returns a stable English name for a byte-order value.
constexpr const char* byte_order_name(byte_order value) noexcept {
    switch (value) {
    case byte_order::little_endian: return "little-endian";
    case byte_order::big_endian: return "big-endian";
    case byte_order::mixed_endian: return "mixed-endian";
    case byte_order::unknown: return "unknown";
    }
    return "unknown";
}

/// Returns a stable English name for a data-model value.
constexpr const char* data_model_name(data_model value) noexcept {
    switch (value) {
    case data_model::lp32: return "LP32";
    case data_model::ilp32: return "ILP32";
    case data_model::lp64: return "LP64";
    case data_model::llp64: return "LLP64";
    case data_model::ilp64: return "ILP64";
    case data_model::other: return "other";
    case data_model::unknown: return "unknown";
    }
    return "unknown";
}

} // namespace syscape

#endif
