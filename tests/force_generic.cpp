#include <syscape/architecture.hpp>
#include <syscape/execution_environment.hpp>

int main() {
    if (syscape::target_operating_system() != syscape::operating_system::unknown) {
        return 1;
    }
    if (syscape::target_architecture() == syscape::architecture::unknown) {
        return 2;
    }
    return 0;
}
