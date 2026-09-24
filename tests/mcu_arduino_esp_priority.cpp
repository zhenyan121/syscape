#include <syscape/board.hpp>
#include <syscape/execution_environment.hpp>

// Verifies that when both ARDUINO and an explicit hardware architecture
// (ESP_PLATFORM) are defined, target_board_family() prioritizes espressif_esp
// over generic arduino.
static_assert(syscape::target_board_family() ==
                  syscape::board_family::espressif_esp,
              "ESP_PLATFORM must take precedence over ARDUINO");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::bare_metal,
              "MCU target must select bare_metal");

int main() {
    return 0;
}
