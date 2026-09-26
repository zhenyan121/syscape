#include <syscape/toolchain.hpp>

int main() {
#if defined(TEST_INTEL_CLASSIC)
    static_assert(syscape::target_compiler() ==
                      syscape::compiler::intel_classic,
                  "compiler must be intel_classic");
    static_assert(syscape::target_compiler_version().major == 19,
                  "major == 19");
    static_assert(syscape::target_compiler_version().minor == 0, "minor == 0");
    static_assert(syscape::target_compiler_version().patch == 2, "patch == 2");
#elif defined(TEST_INTEL_LLVM)
    static_assert(syscape::target_compiler() == syscape::compiler::intel_llvm,
                  "compiler must be intel_llvm");
    static_assert(syscape::target_compiler_version().major == 2023,
                  "major == 2023");
    static_assert(syscape::target_compiler_version().minor == 2, "minor == 2");
    static_assert(syscape::target_compiler_version().patch == 1, "patch == 1");
#elif defined(TEST_IBM_OPEN_XL)
    static_assert(syscape::target_compiler() == syscape::compiler::ibm_open_xl,
                  "compiler must be ibm_open_xl");
    static_assert(syscape::target_compiler_version().major == 17,
                  "major == 17");
    static_assert(syscape::target_compiler_version().minor == 1, "minor == 1");
    static_assert(syscape::target_compiler_version().patch == 2, "patch == 2");
#elif defined(TEST_IBM_XL)
    static_assert(syscape::target_compiler() == syscape::compiler::ibm_xl,
                  "compiler must be ibm_xl");
    static_assert(syscape::target_compiler_version().major == 13,
                  "major == 13");
    static_assert(syscape::target_compiler_version().minor == 1, "minor == 1");
    static_assert(syscape::target_compiler_version().patch == 2, "patch == 2");
#elif defined(TEST_IBM_XL_LEGACY)
    static_assert(syscape::target_compiler() == syscape::compiler::ibm_xl,
                  "compiler must be ibm_xl");
    static_assert(syscape::target_compiler_version().major == 12,
                  "major == 12");
    static_assert(syscape::target_compiler_version().minor == 1, "minor == 1");
    static_assert(syscape::target_compiler_version().patch == 0, "patch == 0");
#elif defined(TEST_SUNPRO)
    static_assert(syscape::target_compiler() ==
                      syscape::compiler::oracle_developer_studio,
                  "compiler must be oracle_developer_studio");
    static_assert(syscape::target_compiler_version().major == 5, "major == 5");
    static_assert(syscape::target_compiler_version().minor == 15,
                  "minor == 15");
    static_assert(syscape::target_compiler_version().patch == 0, "patch == 0");
#elif defined(TEST_SUNPRO_LEGACY)
    static_assert(syscape::target_compiler() ==
                      syscape::compiler::oracle_developer_studio,
                  "compiler must be oracle_developer_studio");
    static_assert(syscape::target_compiler_version().major == 5, "major == 5");
    static_assert(syscape::target_compiler_version().minor == 9, "minor == 9");
    static_assert(syscape::target_compiler_version().patch == 0, "patch == 0");
#elif defined(TEST_HP_ACC)
    static_assert(syscape::target_compiler() == syscape::compiler::hp_acc,
                  "compiler must be hp_acc");
    static_assert(syscape::target_compiler_version().major == 6, "major == 6");
    static_assert(syscape::target_compiler_version().minor == 25,
                  "minor == 25");
    static_assert(syscape::target_compiler_version().patch == 0, "patch == 0");
#elif defined(TEST_IAR)
    static_assert(syscape::target_compiler() == syscape::compiler::iar,
                  "compiler must be iar");
    static_assert(syscape::target_compiler_version().major == 8, "major == 8");
    static_assert(syscape::target_compiler_version().minor == 50,
                  "minor == 50");
    static_assert(syscape::target_compiler_version().patch == 1, "patch == 1");
#elif defined(TEST_IAR_LEGACY)
    static_assert(syscape::target_compiler() == syscape::compiler::iar,
                  "compiler must be iar");
    static_assert(syscape::target_compiler_version().major == 3, "major == 3");
    static_assert(syscape::target_compiler_version().minor == 34,
                  "minor == 34");
    static_assert(syscape::target_compiler_version().patch == 0, "patch == 0");
#elif defined(TEST_ARM_AC6)
    static_assert(syscape::target_compiler() == syscape::compiler::arm_compiler,
                  "compiler must be arm_compiler");
    static_assert(syscape::target_compiler_version().major == 6, "major == 6");
    static_assert(syscape::target_compiler_version().minor == 16,
                  "minor == 16");
    static_assert(syscape::target_compiler_version().patch == 1, "patch == 1");
#elif defined(TEST_ARM_AC5)
    static_assert(syscape::target_compiler() == syscape::compiler::arm_compiler,
                  "compiler must be arm_compiler");
    static_assert(syscape::target_compiler_version().major == 5, "major == 5");
    static_assert(syscape::target_compiler_version().minor == 6, "minor == 6");
    static_assert(syscape::target_compiler_version().patch == 7, "patch == 7");
#elif defined(TEST_ARM_CC_ARM_ONLY)
    static_assert(syscape::target_compiler() == syscape::compiler::arm_compiler,
                  "compiler must be arm_compiler");
    static_assert(syscape::target_compiler_version().major == 0, "major == 0");
    static_assert(syscape::target_compiler_version().minor == 0, "minor == 0");
    static_assert(syscape::target_compiler_version().patch == 0, "patch == 0");
#elif defined(TEST_GHS)
    static_assert(syscape::target_compiler() == syscape::compiler::green_hills,
                  "compiler must be green_hills");
    static_assert(syscape::target_compiler_version().major == 2022,
                  "major == 2022");
    static_assert(syscape::target_compiler_version().minor == 1, "minor == 1");
    static_assert(syscape::target_compiler_version().patch == 4, "patch == 4");
#elif defined(TEST_TI)
    static_assert(syscape::target_compiler() ==
                      syscape::compiler::texas_instruments,
                  "compiler must be texas_instruments");
    static_assert(syscape::target_compiler_version().major == 20,
                  "major == 20");
    static_assert(syscape::target_compiler_version().minor == 2, "minor == 2");
    static_assert(syscape::target_compiler_version().patch == 5, "patch == 5");
#elif defined(TEST_RENESAS)
    static_assert(syscape::target_compiler() == syscape::compiler::renesas,
                  "compiler must be renesas");
    static_assert(syscape::target_compiler_version().major == 3, "major == 3");
    static_assert(syscape::target_compiler_version().minor == 1, "minor == 1");
    static_assert(syscape::target_compiler_version().patch == 2, "patch == 2");
#elif defined(TEST_RENESAS_LEGACY)
    static_assert(syscape::target_compiler() == syscape::compiler::renesas,
                  "compiler must be renesas");
    static_assert(syscape::target_compiler_version().major == 2, "major == 2");
    static_assert(syscape::target_compiler_version().minor == 3, "minor == 3");
    static_assert(syscape::target_compiler_version().patch == 0, "patch == 0");
#elif defined(TEST_MICROCHIP_XC32)
    static_assert(syscape::target_compiler() == syscape::compiler::microchip_xc,
                  "compiler must be microchip_xc");
    static_assert(syscape::target_compiler_version().major == 2, "major == 2");
    static_assert(syscape::target_compiler_version().minor == 32,
                  "minor == 32");
    static_assert(syscape::target_compiler_version().patch == 1, "patch == 1");
#elif defined(TEST_MICROCHIP_XC8)
    static_assert(syscape::target_compiler() == syscape::compiler::microchip_xc,
                  "compiler must be microchip_xc");
    static_assert(syscape::target_compiler_version().major == 2, "major == 2");
    static_assert(syscape::target_compiler_version().minor == 31,
                  "minor == 31");
    static_assert(syscape::target_compiler_version().patch == 0, "patch == 0");
#elif defined(TEST_OPENWATCOM)
    static_assert(syscape::target_compiler() == syscape::compiler::open_watcom,
                  "compiler must be open_watcom");
    static_assert(syscape::target_compiler_version().major == 1, "major == 1");
    static_assert(syscape::target_compiler_version().minor == 8, "minor == 8");
    static_assert(syscape::target_compiler_version().patch == 0, "patch == 0");
#elif defined(TEST_OPENWATCOM_V2)
    static_assert(syscape::target_compiler() == syscape::compiler::open_watcom,
                  "compiler must be open_watcom");
    static_assert(syscape::target_compiler_version().major == 2, "major == 2");
    static_assert(syscape::target_compiler_version().minor == 0, "minor == 0");
    static_assert(syscape::target_compiler_version().patch == 0, "patch == 0");
#elif defined(TEST_WATCOM_LEGACY)
    static_assert(syscape::target_compiler() == syscape::compiler::open_watcom,
                  "compiler must be open_watcom");
    static_assert(syscape::target_compiler_version().major == 11,
                  "major == 11");
    static_assert(syscape::target_compiler_version().minor == 0, "minor == 0");
    static_assert(syscape::target_compiler_version().patch == 0, "patch == 0");
#endif
    const syscape::toolchain_version v = syscape::target_compiler_version();
    const syscape::toolchain_version copy = v;
    if (!(v == copy)) {
        return 1;
    }
    if (v != copy) {
        return 2;
    }
    return 0;
}
