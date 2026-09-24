#include <cstdint>
#include <cstring>

#define SYSCAPE_STATIC_BOARD_PROVIDER test_static_provider
#include <syscape/board.hpp>

namespace {

syscape::board_family bsp_family() noexcept {
    return syscape::board_family::microchip_avr;
}

const char* bsp_model() noexcept {
    return "Arduino Mega 2560";
}

std::uint32_t bsp_cpu_freq() noexcept {
    return 16000000U;
}

} // namespace

extern const syscape::board_provider test_static_provider = {
    &bsp_family, &bsp_model, nullptr, &bsp_cpu_freq, nullptr,
    nullptr,     nullptr,    nullptr, nullptr};

int main() {
    // 1. Without dynamic registration, static provider takes effect
    if (syscape::current_board_provider() != &test_static_provider) {
        return 1;
    }
    if (syscape::current_board_family() !=
        syscape::board_family::microchip_avr) {
        return 2;
    }
    if (syscape::board::model() == nullptr ||
        std::strcmp(syscape::board::model(), "Arduino Mega 2560") != 0) {
        return 3;
    }
    if (syscape::board::cpu_frequency_hz() != 16000000U) {
        return 4;
    }

    // 2. Dynamic registration overrides the static provider
    const syscape::board_provider override_prov = {nullptr, nullptr, nullptr,
                                                   nullptr, nullptr, nullptr,
                                                   nullptr, nullptr, nullptr};
    syscape::register_board_provider(&override_prov);
    if (syscape::current_board_provider() != &override_prov) {
        return 5;
    }

    // 3. Clearing dynamic provider falls back to static provider
    syscape::clear_board_provider();
    if (syscape::current_board_provider() != &test_static_provider) {
        return 6;
    }
    if (syscape::current_board_family() !=
        syscape::board_family::microchip_avr) {
        return 7;
    }

    return 0;
}
