function(syscape_configure_test target standard library_target)
    target_link_libraries(${target} PRIVATE ${library_target})
    set_target_properties(
        ${target}
        PROPERTIES
            CXX_STANDARD ${standard}
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(
            ${target}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -pedantic-errors
                -Wconversion
                -Wsign-conversion
                -Werror)
    endif()
endfunction()

function(syscape_add_test target standard library_target)
    add_executable(${target} ${ARGN})
    syscape_configure_test(${target} ${standard} ${library_target})
    add_test(NAME ${target} COMMAND ${target})
endfunction()

# Freestanding Minimal and other allocation-free foundation headers use C++11.
syscape_add_test(header_architecture 11 syscape::syscape header_architecture.cpp)
syscape_add_test(header_capability 11 syscape::syscape header_capability.cpp)
syscape_add_test(
    header_execution_environment
    11
    syscape::syscape
    header_execution_environment.cpp)
syscape_add_test(header_toolchain 11 syscape::syscape header_toolchain.cpp)

foreach(standard IN ITEMS 11 14 17)
    syscape_add_test(
        minimal_language_${standard}
        ${standard}
        syscape::syscape
        minimal_language.cpp)
endforeach()

syscape_add_test(
    minimal_odr
    11
    syscape::syscape
    minimal_odr_main.cpp
    minimal_odr_other.cpp)

syscape_add_test(force_generic 11 syscape::syscape force_generic.cpp)
target_compile_definitions(force_generic PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(force_unknown 11 syscape::syscape force_unknown.cpp)
target_compile_definitions(force_unknown PRIVATE SYSCAPE_FORCE_UNKNOWN_TARGET=1)

# The shared hosted error foundation is independently usable with C++11.
syscape_add_test(header_error 11 syscape::syscape header_error.cpp)

# Hosted Full headers and runtime queries retain their C++17 contract.
syscape_add_test(header_result 17 syscape::hosted header_result.cpp)
syscape_add_test(header_os 17 syscape::hosted header_os.cpp)
syscape_add_test(header_cpu 17 syscape::hosted header_cpu.cpp)
syscape_add_test(header_memory 17 syscape::hosted header_memory.cpp)
syscape_add_test(header_process 17 syscape::hosted header_process.cpp)
syscape_add_test(header_user 17 syscape::hosted header_user.cpp)
syscape_add_test(header_filesystem 17 syscape::hosted header_filesystem.cpp)
syscape_add_test(header_network 17 syscape::hosted header_network.cpp)
syscape_add_test(header_locale 17 syscape::hosted header_locale.cpp)
syscape_add_test(header_environment 17 syscape::hosted header_environment.cpp)
syscape_add_test(header_resource 17 syscape::hosted header_resource.cpp)
syscape_add_test(foundation 17 syscape::hosted foundation.cpp)
syscape_add_test(odr 17 syscape::hosted odr_main.cpp odr_other.cpp)

syscape_add_test(os_force_generic 17 syscape::hosted os_force_generic.cpp)
target_compile_definitions(os_force_generic PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(cpu_force_generic 17 syscape::hosted cpu_force_generic.cpp)
target_compile_definitions(cpu_force_generic PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(memory_force_generic 17 syscape::hosted memory_force_generic.cpp)
target_compile_definitions(memory_force_generic PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(process_force_generic 17 syscape::hosted process_force_generic.cpp)
target_compile_definitions(
    process_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(user_force_generic 17 syscape::hosted user_force_generic.cpp)
target_compile_definitions(
    user_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(filesystem_force_generic 17 syscape::hosted filesystem_force_generic.cpp)
target_compile_definitions(
    filesystem_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(network_force_generic 17 syscape::hosted network_force_generic.cpp)
target_compile_definitions(
    network_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(locale_force_generic 17 syscape::hosted locale_force_generic.cpp)
target_compile_definitions(
    locale_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(environment_force_generic 17 syscape::hosted environment_force_generic.cpp)
target_compile_definitions(
    environment_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(resource_force_generic 17 syscape::hosted resource_force_generic.cpp)
target_compile_definitions(
    resource_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    syscape_add_test(os_linux 17 syscape::hosted os_linux.cpp)
    syscape_add_test(cpu_linux 17 syscape::hosted cpu_linux.cpp)
    syscape_add_test(memory_linux 17 syscape::hosted memory_linux.cpp)
    syscape_add_test(process_linux 17 syscape::hosted process_linux.cpp)
    syscape_add_test(user_linux 17 syscape::hosted user_linux.cpp)
    syscape_add_test(filesystem_linux 17 syscape::hosted filesystem_linux.cpp)
    syscape_add_test(network_linux 17 syscape::hosted network_linux.cpp)
    syscape_add_test(locale_linux 17 syscape::hosted locale_linux.cpp)
    syscape_add_test(environment_linux 17 syscape::hosted environment_linux.cpp)
    syscape_add_test(resource_linux 17 syscape::hosted resource_linux.cpp)
endif()

if(WIN32)
    syscape_add_test(cpu_windows 17 syscape::hosted cpu_windows.cpp)
    syscape_add_test(memory_windows 17 syscape::hosted memory_windows.cpp)
    syscape_add_test(process_windows 17 syscape::hosted process_windows.cpp)
    syscape_add_test(user_windows 17 syscape::hosted user_windows.cpp)
    syscape_add_test(filesystem_windows 17 syscape::hosted filesystem_windows.cpp)
    syscape_add_test(network_windows 17 syscape::hosted network_windows.cpp)
    syscape_add_test(locale_windows 17 syscape::hosted locale_windows.cpp)
    syscape_add_test(environment_windows 17 syscape::hosted environment_windows.cpp)
    syscape_add_test(resource_windows 17 syscape::hosted resource_windows.cpp)
    # Targets that call the resource performance API follow its documented
    # Windows linkage contract (Kernel32.lib on Windows 7 or later SDKs,
    # otherwise Psapi.lib) without imposing it on unrelated Hosted Full
    # consumers.
    target_link_libraries(resource_windows PRIVATE psapi)
    # Targets that call the CPU power information API follow its documented
    # Windows linkage contract without imposing PowrProf on unrelated Hosted
    # Full consumers.
    target_link_libraries(cpu_windows PRIVATE powrprof)
    # Targets that call the network API follow its documented Windows linkage
    # contract without imposing Iphlpapi on unrelated Hosted Full consumers.
    target_link_libraries(odr PRIVATE iphlpapi)
    target_link_libraries(network_windows PRIVATE iphlpapi)
    # Targets that call the user privilege API follow its documented Windows
    # linkage contract without imposing Advapi32 on unrelated consumers.
    target_link_libraries(header_user PRIVATE advapi32)
    target_link_libraries(user_windows PRIVATE advapi32)
    target_link_libraries(odr PRIVATE advapi32)
endif()

if(APPLE)
    syscape_add_test(cpu_macos 17 syscape::hosted cpu_macos.cpp)
    syscape_add_test(memory_macos 17 syscape::hosted memory_macos.cpp)
    syscape_add_test(process_macos 17 syscape::hosted process_macos.cpp)
    syscape_add_test(user_macos 17 syscape::hosted user_macos.cpp)
    syscape_add_test(filesystem_macos 17 syscape::hosted filesystem_macos.cpp)
    syscape_add_test(network_macos 17 syscape::hosted network_macos.cpp)
    syscape_add_test(locale_macos 17 syscape::hosted locale_macos.cpp)
    syscape_add_test(environment_macos 17 syscape::hosted environment_macos.cpp)
    syscape_add_test(resource_macos 17 syscape::hosted resource_macos.cpp)
endif()

# These smoke tests verify the language requirements propagated by each public
# CMake interface target. The hosted target must upgrade a C++11 consumer.
syscape_add_test(cmake_core_standard 11 syscape::syscape cmake_core_standard.cpp)
syscape_add_test(cmake_hosted_standard 11 syscape::hosted cmake_hosted_standard.cpp)

foreach(standard IN ITEMS 11 14 17)
    add_library(
        freestanding_headers_${standard}
        OBJECT
        freestanding_headers.cpp)
    syscape_configure_test(
        freestanding_headers_${standard}
        ${standard}
        syscape::syscape)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            freestanding_headers_${standard}
            PRIVATE
                -ffreestanding)
    endif()
endforeach()

function(syscape_expect_cpp11_rejection header expected_message)
    string(REPLACE "/" "_" probe_name "${header}")
    string(REPLACE "." "_" probe_name "${probe_name}")
    set(source "${CMAKE_CURRENT_BINARY_DIR}/${probe_name}_cpp11.cpp")
    file(WRITE "${source}" "#include <${header}>\nint main() { return 0; }\n")
    try_compile(
        accepted
        "${CMAKE_CURRENT_BINARY_DIR}/${probe_name}_cpp11"
        SOURCES "${source}"
        CMAKE_FLAGS
            "-DCMAKE_CXX_STANDARD=11"
            "-DCMAKE_CXX_STANDARD_REQUIRED=ON"
            "-DCMAKE_CXX_EXTENSIONS=OFF"
            "-DINCLUDE_DIRECTORIES=${PROJECT_SOURCE_DIR}/include"
        OUTPUT_VARIABLE output)
    if(accepted)
        message(FATAL_ERROR "${header} unexpectedly compiled as C++11")
    endif()
    string(FIND "${output}" "${expected_message}" message_position)
    if(message_position EQUAL -1)
        message(FATAL_ERROR
            "${header} failed as C++11 without the expected diagnostic:\n${output}")
    endif()
endfunction()

syscape_expect_cpp11_rejection(
    "syscape/result.hpp"
    "syscape/result.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/os.hpp"
    "syscape/os.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/cpu.hpp"
    "syscape/cpu.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/memory.hpp"
    "syscape/memory.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/process.hpp"
    "syscape/process.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/user.hpp"
    "syscape/user.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/filesystem.hpp"
    "syscape/filesystem.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/network.hpp"
    "syscape/network.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/locale.hpp"
    "syscape/locale.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/environment.hpp"
    "syscape/environment.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/resource.hpp"
    "syscape/resource.hpp requires C++17 or later")
