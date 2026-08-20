#include <syscape/architecture.hpp>
#include <syscape/architecture.hpp>

int main() {
    const syscape::data_model_info model = syscape::target_data_model_info();
    return model.pointer_bits == 0U ||
                   syscape::architecture_name(syscape::target_architecture()) == nullptr
               ? 1
               : 0;
}
