#include <syscape/capability.hpp>
#include <syscape/capability.hpp>

int main() {
    const syscape::capability value(syscape::capability_state::available);
    return value.available() && value.recognized() ? 0 : 1;
}
