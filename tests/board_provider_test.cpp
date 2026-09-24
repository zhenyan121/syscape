#include <cstdint>
#include <cstring>
#include <syscape/board.hpp>

namespace {

syscape::board_family mock_family() noexcept {
    return syscape::board_family::raspberry_pi_rp;
}

const char* mock_model() noexcept {
    return "Raspberry Pi Pico";
}

const char* mock_manufacturer() noexcept {
    return "Raspberry Pi Foundation";
}

std::uint32_t mock_cpu_frequency_hz() noexcept {
    return 133000000U;
}

std::size_t mock_flash_size_bytes() noexcept {
    return 2097152U;
}

std::size_t mock_sram_size_bytes() noexcept {
    return 266240U;
}

std::size_t mock_eeprom_size_bytes() noexcept {
    return 0U;
}

unsigned int mock_gpio_pin_count() noexcept {
    return 30U;
}

std::uint32_t mock_supply_voltage_mv() noexcept {
    return 3300U;
}

syscape::board::board_provider full_provider = {&mock_family,
                                                &mock_model,
                                                &mock_manufacturer,
                                                &mock_cpu_frequency_hz,
                                                &mock_flash_size_bytes,
                                                &mock_sram_size_bytes,
                                                &mock_eeprom_size_bytes,
                                                &mock_gpio_pin_count,
                                                &mock_supply_voltage_mv};

syscape::board_family partial_family() noexcept {
    return syscape::board_family::st_stm32;
}

const char* partial_model() noexcept {
    return "STM32F4-Discovery";
}

syscape::board::board_provider partial_provider = {
    &partial_family, &partial_model, nullptr, nullptr, nullptr,
    nullptr,         nullptr,        nullptr, nullptr};

} // namespace

int main() {
    // 1. Initial unregistered default state
    syscape::clear_board_provider();
    if (syscape::current_board_provider() != nullptr) {
        return 1;
    }
    if (syscape::board::model() != nullptr) {
        return 2;
    }
    if (syscape::board::manufacturer() != nullptr) {
        return 3;
    }
    if (syscape::board::flash_size_bytes() != 0) {
        return 4;
    }
    if (syscape::board::sram_size_bytes() != 0) {
        return 5;
    }
    if (syscape::board::eeprom_size_bytes() != 0) {
        return 6;
    }
    if (syscape::board::gpio_pin_count() != 0) {
        return 7;
    }
    if (syscape::board::supply_voltage_mv() != 0) {
        return 8;
    }
    if (syscape::board::current_board_family() !=
        syscape::target_board_family()) {
        return 9;
    }

    const syscape::board_info default_info = syscape::board::info();
    if (default_info.family != syscape::target_board_family()) {
        return 10;
    }
    if (default_info.model != nullptr || default_info.manufacturer != nullptr) {
        return 11;
    }

    // 2. Full provider registration and verification
    syscape::register_board_provider(&full_provider);
    if (syscape::current_board_provider() != &full_provider) {
        return 12;
    }
    if (syscape::board::current_board_family() !=
        syscape::board_family::raspberry_pi_rp) {
        return 13;
    }
    if (syscape::board::model() == nullptr ||
        std::strcmp(syscape::board::model(), "Raspberry Pi Pico") != 0) {
        return 14;
    }
    if (syscape::board::manufacturer() == nullptr ||
        std::strcmp(syscape::board::manufacturer(),
                    "Raspberry Pi Foundation") != 0) {
        return 15;
    }
    if (syscape::board::cpu_frequency_hz() != 133000000U) {
        return 16;
    }
    if (syscape::board::flash_size_bytes() != 2097152U) {
        return 17;
    }
    if (syscape::board::sram_size_bytes() != 266240U) {
        return 18;
    }
    if (syscape::board::eeprom_size_bytes() != 0U) {
        return 19;
    }
    if (syscape::board::gpio_pin_count() != 30U) {
        return 20;
    }
    if (syscape::board::supply_voltage_mv() != 3300U) {
        return 21;
    }

    const syscape::board_info full_info = syscape::board::info();
    if (full_info.family != syscape::board_family::raspberry_pi_rp) {
        return 22;
    }
    if (full_info.cpu_frequency_hz != 133000000U ||
        full_info.flash_size_bytes != 2097152U ||
        full_info.sram_size_bytes != 266240U ||
        full_info.gpio_pin_count != 30U ||
        full_info.supply_voltage_mv != 3300U) {
        return 23;
    }

    // 3. Partial provider with null callbacks
    syscape::register_board_provider(&partial_provider);
    if (syscape::board::current_board_family() !=
        syscape::board_family::st_stm32) {
        return 24;
    }
    if (syscape::board::model() == nullptr ||
        std::strcmp(syscape::board::model(), "STM32F4-Discovery") != 0) {
        return 25;
    }
    if (syscape::board::manufacturer() != nullptr) {
        return 26;
    }
    if (syscape::board::flash_size_bytes() != 0U) {
        return 27;
    }

    // 4. Clear provider and verify reset
    syscape::clear_board_provider();
    if (syscape::current_board_provider() != nullptr) {
        return 28;
    }
    if (syscape::board::model() != nullptr) {
        return 29;
    }
    if (syscape::board::current_board_family() !=
        syscape::target_board_family()) {
        return 30;
    }

    return 0;
}
