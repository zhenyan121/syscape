#include <syscape/toolchain.hpp>

#include <cassert>
#include <cstring>

static void test_compiler_names() {
    assert(std::strcmp(syscape::compiler_name(syscape::compiler::unknown),
                       "unknown") == 0);
    assert(std::strcmp(syscape::compiler_name(syscape::compiler::gcc), "gcc") ==
           0);
    assert(std::strcmp(syscape::compiler_name(syscape::compiler::clang),
                       "clang") == 0);
    assert(std::strcmp(syscape::compiler_name(syscape::compiler::apple_clang),
                       "apple-clang") == 0);
    assert(std::strcmp(syscape::compiler_name(syscape::compiler::msvc),
                       "msvc") == 0);
    assert(std::strcmp(syscape::compiler_name(syscape::compiler::intel_classic),
                       "intel-classic") == 0);
    assert(std::strcmp(syscape::compiler_name(syscape::compiler::intel_llvm),
                       "intel-llvm") == 0);
    assert(std::strcmp(syscape::compiler_name(syscape::compiler::ibm_xl),
                       "ibm-xl") == 0);
    assert(std::strcmp(syscape::compiler_name(syscape::compiler::ibm_open_xl),
                       "ibm-open-xl") == 0);
    assert(std::strcmp(syscape::compiler_name(
                           syscape::compiler::oracle_developer_studio),
                       "oracle-developer-studio") == 0);
    assert(std::strcmp(syscape::compiler_name(syscape::compiler::hp_acc),
                       "hp-acc") == 0);
    assert(std::strcmp(syscape::compiler_name(syscape::compiler::iar), "iar") ==
           0);
    assert(std::strcmp(syscape::compiler_name(syscape::compiler::arm_compiler),
                       "arm-compiler") == 0);
    assert(std::strcmp(syscape::compiler_name(syscape::compiler::green_hills),
                       "green-hills") == 0);
    assert(std::strcmp(
               syscape::compiler_name(syscape::compiler::texas_instruments),
               "texas-instruments") == 0);
    assert(std::strcmp(syscape::compiler_name(syscape::compiler::renesas),
                       "renesas") == 0);
    assert(std::strcmp(syscape::compiler_name(syscape::compiler::microchip_xc),
                       "microchip-xc") == 0);
    assert(std::strcmp(syscape::compiler_name(syscape::compiler::open_watcom),
                       "open-watcom") == 0);
    assert(std::strcmp(syscape::compiler_name(syscape::compiler::emscripten),
                       "emscripten") == 0);
}

static void test_standard_library_names() {
    assert(std::strcmp(syscape::standard_library_name(
                           syscape::standard_library::unknown),
                       "unknown") == 0);
    assert(std::strcmp(syscape::standard_library_name(
                           syscape::standard_library::libstdcxx),
                       "libstdc++") == 0);
    assert(std::strcmp(syscape::standard_library_name(
                           syscape::standard_library::libcxx),
                       "libc++") == 0);
    assert(std::strcmp(syscape::standard_library_name(
                           syscape::standard_library::msvc_stl),
                       "msvc-stl") == 0);
    assert(std::strcmp(syscape::standard_library_name(
                           syscape::standard_library::dinkumware),
                       "dinkumware") == 0);
}

static void test_version_operators() {
    constexpr syscape::toolchain_version v1 {1, 2, 3};
    constexpr syscape::toolchain_version v2 {1, 2, 3};
    constexpr syscape::toolchain_version v_major_diff {2, 2, 3};
    constexpr syscape::toolchain_version v_minor_diff {1, 3, 3};
    constexpr syscape::toolchain_version v_patch_diff {1, 2, 4};

    static_assert(v1 == v2, "equality operator");
    static_assert(!(v1 != v2), "inequality operator");
    static_assert(v1 != v_major_diff, "major difference inequality");
    static_assert(v1 != v_minor_diff, "minor difference inequality");
    static_assert(v1 != v_patch_diff, "patch difference inequality");
    static_assert(!(v1 == v_major_diff), "major difference equality");
    static_assert(!(v1 == v_minor_diff), "minor difference equality");
    static_assert(!(v1 == v_patch_diff), "patch difference equality");
}

int main() {
    test_compiler_names();
    test_standard_library_names();
    test_version_operators();

    const syscape::compiler comp = syscape::target_compiler();
    assert(comp != syscape::compiler::unknown);

    const syscape::toolchain_version ver = syscape::target_compiler_version();
    assert(ver.major > 0U);

    const long cpp_ver = syscape::target_cpp_version();
    assert(cpp_ver >= 201103L);

    return 0;
}
