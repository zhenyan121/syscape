#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::emscripten,
              "__EMSCRIPTEN__ must select Emscripten");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::sandboxed,
              "Emscripten must select the sandboxed execution environment");

int main() {
    return 0;
}
