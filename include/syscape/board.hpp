#ifndef SYSCAPE_BOARD_HPP
#define SYSCAPE_BOARD_HPP

/// @file
/// @brief Allocation-free compile-target MCU and board information, family
/// classification, and explicit board provider interface.
/// @note Minimum compatibility profile: Freestanding Minimal.
/// @note Minimum language version: C++11; no hosted library or dynamic
/// allocation is required.
///
/// This header provides:
/// - Compile-time classification of MCU and board families (board_family).
/// - An explicit, allocation-free board provider interface (board_provider)
///   for board support packages (BSPs), runtime firmware, and applications.
/// - In-process board information snapshots (board_info) capturing CPU
///   frequency, flash size, SRAM size, EEPROM size, GPIO pin count, and supply
///   voltage without heap allocation or exceptions.
/// - Support for static compile-time provider binding via
///   SYSCAPE_STATIC_BOARD_PROVIDER or runtime dynamic registration via
///   register_board_provider().

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201103L
#error "syscape/board.hpp requires C++11 or later"
#endif

#include <cstddef>
#include <cstdint>

namespace syscape {

/// Identifies the microcontroller or board family selected at compile time or
/// reported by an application-supplied board provider.
enum class board_family : std::uint16_t {
    /// The MCU or board family could not be determined.
    unknown = 0,
    /// Arduino framework and ecosystem boards.
    arduino,
    /// Microchip / Atmel AVR 8-bit MCU family (e.g. ATmega328P, ATmega2560,
    /// ATtiny).
    microchip_avr,
    /// Microchip SAM Arm Cortex-M MCU family (e.g. SAMD21, SAMD51, SAM3X).
    microchip_sam,
    /// Espressif Systems ESP MCU family (e.g. ESP8266, ESP32, ESP32-S/C
    /// series).
    espressif_esp,
    /// STMicroelectronics STM32 Arm Cortex-M MCU family (e.g. F0..F7, G0..G4,
    /// H5/H7, L0..L5, U5).
    st_stm32,
    /// Raspberry Pi microcontroller family (e.g. RP2040, RP2350).
    raspberry_pi_rp,
    /// Nordic Semiconductor nRF wireless SoC family (e.g. nRF51, nRF52, nRF53,
    /// nRF54).
    nordic_nrf,
    /// Texas Instruments MCU and DSP family (e.g. MSP430, TMS320, SimpleLink
    /// CC13xx/CC26xx).
    ti_mcu,
    /// NXP Semiconductors MCU family (e.g. LPC series, i.MX RT crossover MCUs,
    /// Kinetis).
    nxp_mcu,
    /// Renesas Electronics MCU family (e.g. RX, RA, RZ).
    renesas_mcu,
    /// Microchip PIC 32-bit and 16-bit MCU family (e.g. PIC32MX, PIC32MZ,
    /// dsPIC33).
    microchip_pic,
    /// SiFive and other commercial RISC-V SoC/core families (e.g. FE310, U540).
    sifive_riscv,
    /// GigaDevice GD32 MCU family (Arm Cortex-M and RISC-V variants).
    gigadevice_gd32,
    /// WCH (WinChipHead) CH32 MCU family (RISC-V and Arm Cortex-M variants).
    wch_ch32,
    /// Bouffalo Lab BL-series wireless MCU family (e.g. BL602, BL616, BL702,
    /// BL808).
    bouffalo_bl,
    /// Explicit generic or application-defined custom board.
    generic
};

/// Returns a stable lowercase ASCII identifier for a board family.
SYSCAPE_DETAIL_CONSTEXPR14 const char*
board_family_name(board_family value) noexcept {
    switch (value) {
    case board_family::arduino:
        return "arduino";
    case board_family::microchip_avr:
        return "microchip_avr";
    case board_family::microchip_sam:
        return "microchip_sam";
    case board_family::espressif_esp:
        return "espressif_esp";
    case board_family::st_stm32:
        return "st_stm32";
    case board_family::raspberry_pi_rp:
        return "raspberry_pi_rp";
    case board_family::nordic_nrf:
        return "nordic_nrf";
    case board_family::ti_mcu:
        return "ti_mcu";
    case board_family::nxp_mcu:
        return "nxp_mcu";
    case board_family::renesas_mcu:
        return "renesas_mcu";
    case board_family::microchip_pic:
        return "microchip_pic";
    case board_family::sifive_riscv:
        return "sifive_riscv";
    case board_family::gigadevice_gd32:
        return "gigadevice_gd32";
    case board_family::wch_ch32:
        return "wch_ch32";
    case board_family::bouffalo_bl:
        return "bouffalo_bl";
    case board_family::generic:
        return "generic";
    case board_family::unknown:
        return "unknown";
    }
    return "unknown";
}

/// Returns the MCU or board family identified by the toolchain or target
/// configuration macros for this translation unit.
constexpr board_family target_board_family() noexcept {
#if defined(SYSCAPE_FORCE_GENERIC_BACKEND) ||                                  \
    defined(SYSCAPE_FORCE_UNKNOWN_TARGET)
    return board_family::unknown;
#elif defined(SYSCAPE_TARGET_MCU_AVR)
    return board_family::microchip_avr;
#elif defined(SYSCAPE_TARGET_MCU_SAM)
    return board_family::microchip_sam;
#elif defined(SYSCAPE_TARGET_MCU_ESP)
    return board_family::espressif_esp;
#elif defined(SYSCAPE_TARGET_MCU_STM32)
    return board_family::st_stm32;
#elif defined(SYSCAPE_TARGET_MCU_RP)
    return board_family::raspberry_pi_rp;
#elif defined(SYSCAPE_TARGET_MCU_NORDIC)
    return board_family::nordic_nrf;
#elif defined(SYSCAPE_TARGET_MCU_TI)
    return board_family::ti_mcu;
#elif defined(SYSCAPE_TARGET_MCU_NXP)
    return board_family::nxp_mcu;
#elif defined(SYSCAPE_TARGET_MCU_RENESAS)
    return board_family::renesas_mcu;
#elif defined(SYSCAPE_TARGET_MCU_PIC)
    return board_family::microchip_pic;
#elif defined(SYSCAPE_TARGET_MCU_GD32)
    return board_family::gigadevice_gd32;
#elif defined(SYSCAPE_TARGET_MCU_CH32)
    return board_family::wch_ch32;
#elif defined(SYSCAPE_TARGET_MCU_BOUFFALO)
    return board_family::bouffalo_bl;
#elif defined(SYSCAPE_TARGET_MCU_SIFIVE)
    return board_family::sifive_riscv;
#elif defined(SYSCAPE_TARGET_MCU_ARDUINO)
    return board_family::arduino;
#else
    return board_family::unknown;
#endif
}

namespace board {

using syscape::board_family;
using syscape::board_family_name;
using syscape::target_board_family;

/// Stores an allocation-free snapshot of board and hardware capabilities.
struct board_info {
    /// The identified or reported board family.
    board_family family;
    /// Product or board model identifier (e.g. "Raspberry Pi Pico", "Arduino
    /// Uno"), or nullptr if unknown.
    const char* model;
    /// Manufacturer or vendor name (e.g. "Raspberry Pi", "STMicroelectronics"),
    /// or nullptr if unknown.
    const char* manufacturer;
    /// Main CPU / core clock frequency in Hertz, or 0 if unknown.
    std::uint32_t cpu_frequency_hz;
    /// Total flash memory size in bytes, or 0 if unknown.
    std::size_t flash_size_bytes;
    /// Total internal SRAM size in bytes, or 0 if unknown.
    std::size_t sram_size_bytes;
    /// Total non-volatile EEPROM size in bytes, or 0 if unknown.
    std::size_t eeprom_size_bytes;
    /// Total accessible GPIO pin count, or 0 if unknown.
    unsigned int gpio_pin_count;
    /// Board nominal operating voltage in millivolts (e.g. 3300 or 5000), or 0
    /// if unknown.
    std::uint32_t supply_voltage_mv;
};

/// Allocation-free provider interface for board support packages and embedded
/// firmware. All callbacks are optional function pointers returning
/// scalar values or string literals.
struct board_provider {
    /// Returns the active board family.
    board_family (*family_fn)();
    /// Returns the board model name.
    const char* (*model_fn)();
    /// Returns the board manufacturer name.
    const char* (*manufacturer_fn)();
    /// Returns the CPU core clock frequency in Hertz.
    std::uint32_t (*cpu_frequency_hz_fn)();
    /// Returns the flash memory capacity in bytes.
    std::size_t (*flash_size_bytes_fn)();
    /// Returns the internal SRAM capacity in bytes.
    std::size_t (*sram_size_bytes_fn)();
    /// Returns the EEPROM capacity in bytes.
    std::size_t (*eeprom_size_bytes_fn)();
    /// Returns the count of usable GPIO pins.
    unsigned int (*gpio_pin_count_fn)();
    /// Returns the nominal supply voltage in millivolts.
    std::uint32_t (*supply_voltage_mv_fn)();
};

} // namespace board

namespace detail {

/// ODR-safe template storage holder for registered board provider in C++11.
template <typename Tag = void>
struct board_provider_holder {
    static const board::board_provider* provider;
};

template <typename Tag>
const board::board_provider* board_provider_holder<Tag>::provider = nullptr;

} // namespace detail
} // namespace syscape

#if defined(SYSCAPE_STATIC_BOARD_PROVIDER)
/// Compile-time statically bound board provider when configured by the build.
extern const ::syscape::board::board_provider SYSCAPE_STATIC_BOARD_PROVIDER;
#endif

namespace syscape {
namespace board {

/// Registers a global board provider for the application or BSP.
/// Passing nullptr clears the active provider.
inline void register_board_provider(const board_provider* provider) noexcept {
    detail::board_provider_holder<>::provider = provider;
}

/// Clears the registered board provider, resetting to default compile-time
/// discovery.
inline void clear_board_provider() noexcept {
    detail::board_provider_holder<>::provider = nullptr;
}

/// Returns the currently active board provider, or nullptr if none is
/// registered.
inline const board_provider* current_board_provider() noexcept {
    const board_provider* registered =
        detail::board_provider_holder<>::provider;
    if (registered != nullptr) {
        return registered;
    }
#if defined(SYSCAPE_STATIC_BOARD_PROVIDER)
    return &(SYSCAPE_STATIC_BOARD_PROVIDER);
#else
    return nullptr;
#endif
}

/// Returns the current board family, querying the registered board provider if
/// available, or falling back to target_board_family().
inline board_family current_board_family() noexcept {
    const board_provider* p = current_board_provider();
    if (p != nullptr && p->family_fn != nullptr) {
        return p->family_fn();
    }
    return target_board_family();
}

/// Returns the board model name from the active provider, or nullptr if
/// unknown.
inline const char* model() noexcept {
    const board_provider* p = current_board_provider();
    if (p != nullptr && p->model_fn != nullptr) {
        return p->model_fn();
    }
    return nullptr;
}

/// Returns the board manufacturer name from the active provider, or nullptr if
/// unknown.
inline const char* manufacturer() noexcept {
    const board_provider* p = current_board_provider();
    if (p != nullptr && p->manufacturer_fn != nullptr) {
        return p->manufacturer_fn();
    }
    return nullptr;
}

/// Returns the CPU clock frequency in Hertz from the active provider or
/// standard compiler clock macros (e.g. F_CPU on AVR), or 0 if unknown.
inline std::uint32_t cpu_frequency_hz() noexcept {
    const board_provider* p = current_board_provider();
    if (p != nullptr && p->cpu_frequency_hz_fn != nullptr) {
        return p->cpu_frequency_hz_fn();
    }
#if defined(F_CPU)
    return static_cast<std::uint32_t>(F_CPU);
#else
    return 0;
#endif
}

/// Returns the total flash memory size in bytes from the active provider, or 0
/// if unknown.
inline std::size_t flash_size_bytes() noexcept {
    const board_provider* p = current_board_provider();
    if (p != nullptr && p->flash_size_bytes_fn != nullptr) {
        return p->flash_size_bytes_fn();
    }
    return 0;
}

/// Returns the total internal SRAM size in bytes from the active provider, or 0
/// if unknown.
inline std::size_t sram_size_bytes() noexcept {
    const board_provider* p = current_board_provider();
    if (p != nullptr && p->sram_size_bytes_fn != nullptr) {
        return p->sram_size_bytes_fn();
    }
    return 0;
}

/// Returns the total non-volatile EEPROM size in bytes from the active
/// provider, or 0 if unknown.
inline std::size_t eeprom_size_bytes() noexcept {
    const board_provider* p = current_board_provider();
    if (p != nullptr && p->eeprom_size_bytes_fn != nullptr) {
        return p->eeprom_size_bytes_fn();
    }
    return 0;
}

/// Returns the count of usable GPIO pins from the active provider, or 0 if
/// unknown.
inline unsigned int gpio_pin_count() noexcept {
    const board_provider* p = current_board_provider();
    if (p != nullptr && p->gpio_pin_count_fn != nullptr) {
        return p->gpio_pin_count_fn();
    }
    return 0;
}

/// Returns the nominal supply voltage in millivolts from the active provider,
/// or 0 if unknown.
inline std::uint32_t supply_voltage_mv() noexcept {
    const board_provider* p = current_board_provider();
    if (p != nullptr && p->supply_voltage_mv_fn != nullptr) {
        return p->supply_voltage_mv_fn();
    }
    return 0;
}

/// Returns a populated board_info structure snapshot.
inline board_info info() noexcept {
    board_info result = {};
    result.family = current_board_family();
    result.model = model();
    result.manufacturer = manufacturer();
    result.cpu_frequency_hz = cpu_frequency_hz();
    result.flash_size_bytes = flash_size_bytes();
    result.sram_size_bytes = sram_size_bytes();
    result.eeprom_size_bytes = eeprom_size_bytes();
    result.gpio_pin_count = gpio_pin_count();
    result.supply_voltage_mv = supply_voltage_mv();
    return result;
}

} // namespace board

using board::board_info;
using board::board_provider;
using board::clear_board_provider;
using board::current_board_family;
using board::current_board_provider;
using board::register_board_provider;

} // namespace syscape

#endif
