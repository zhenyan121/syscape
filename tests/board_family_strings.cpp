#include <cstring>
#include <syscape/board.hpp>

int main() {
    const syscape::board_family families[] = {
        syscape::board_family::unknown,
        syscape::board_family::arduino,
        syscape::board_family::microchip_avr,
        syscape::board_family::microchip_sam,
        syscape::board_family::espressif_esp,
        syscape::board_family::st_stm32,
        syscape::board_family::raspberry_pi_rp,
        syscape::board_family::nordic_nrf,
        syscape::board_family::ti_mcu,
        syscape::board_family::nxp_mcu,
        syscape::board_family::renesas_mcu,
        syscape::board_family::microchip_pic,
        syscape::board_family::sifive_riscv,
        syscape::board_family::gigadevice_gd32,
        syscape::board_family::wch_ch32,
        syscape::board_family::bouffalo_bl,
        syscape::board_family::generic};

    const char* const expected_names[] = {
        "unknown",       "arduino",         "microchip_avr",   "microchip_sam",
        "espressif_esp", "st_stm32",        "raspberry_pi_rp", "nordic_nrf",
        "ti_mcu",        "nxp_mcu",         "renesas_mcu",     "microchip_pic",
        "sifive_riscv",  "gigadevice_gd32", "wch_ch32",        "bouffalo_bl",
        "generic"};

    const std::size_t count = sizeof(families) / sizeof(families[0]);
    for (std::size_t i = 0; i < count; ++i) {
        const char* const name = syscape::board_family_name(families[i]);
        if (name == nullptr) {
            return 1;
        }
        if (std::strcmp(name, expected_names[i]) != 0) {
            return 2;
        }
    }

    return 0;
}
