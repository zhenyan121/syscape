function(syscape_configure_test target)
    target_link_libraries(${target} PRIVATE syscape::syscape)

    if(MSVC)
        target_compile_options(${target} PRIVATE /std:c++17 /W4 /WX /permissive-)
    else()
        target_compile_options(
            ${target}
            PRIVATE
                -std=c++17
                -Wall
                -Wextra
                -Wpedantic
                -pedantic-errors
                -Wconversion
                -Wsign-conversion
                -Werror)
    endif()
endfunction()

function(syscape_add_test target)
    add_executable(${target} ${ARGN})
    syscape_configure_test(${target})
    add_test(NAME ${target} COMMAND ${target})
endfunction()

syscape_add_test(header_architecture header_architecture.cpp)
syscape_add_test(header_capability header_capability.cpp)
syscape_add_test(header_error header_error.cpp)
syscape_add_test(header_execution_environment header_execution_environment.cpp)
syscape_add_test(header_result header_result.cpp)
syscape_add_test(header_toolchain header_toolchain.cpp)
syscape_add_test(foundation foundation.cpp)
syscape_add_test(odr odr_main.cpp odr_other.cpp)

syscape_add_test(force_generic force_generic.cpp)
target_compile_definitions(force_generic PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(force_unknown force_unknown.cpp)
target_compile_definitions(force_unknown PRIVATE SYSCAPE_FORCE_UNKNOWN_TARGET=1)

add_library(freestanding_headers OBJECT freestanding_headers.cpp)
syscape_configure_test(freestanding_headers)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(freestanding_headers PRIVATE -ffreestanding)
endif()
