#include <syscape/board.hpp>
#include <syscape/execution_environment.hpp>

static_assert(syscape::target_board_family() ==
                  syscape::board_family::microchip_avr,
              "AVR macro must select microchip_avr");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::bare_metal,
              "MCU target must select bare_metal");

int main() {
    return 0;
}
