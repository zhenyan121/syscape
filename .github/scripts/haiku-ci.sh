#!/bin/sh

set -eu

build_dir="build-haiku"
test_dir="$build_dir/tests"
example_dir="$build_dir/examples"
common_flags="-Iinclude -Wall -Wextra -Wpedantic -pedantic-errors -Wconversion -Wsign-conversion -Werror"

mkdir -p "$test_dir" "$example_dir"

compile_and_run()
{
    standard="$1"
    output="$2"
    shift 2

    g++ -std="c++$standard" $common_flags "$@" -o "$output"
    "$output"
}

compile_and_run_hosted()
{
    standard="$1"
    output="$2"
    shift 2

    g++ -std="c++$standard" $common_flags "$@" -lnetwork -o "$output"
    "$output"
}

compile_and_run 11 "$test_dir/header_architecture" tests/header_architecture.cpp
compile_and_run 11 "$test_dir/header_capability" tests/header_capability.cpp
compile_and_run 11 "$test_dir/header_execution_environment" \
    tests/header_execution_environment.cpp
compile_and_run 11 "$test_dir/header_toolchain" tests/header_toolchain.cpp
compile_and_run 11 "$test_dir/header_error" tests/header_error.cpp

for standard in 11 14 17; do
    compile_and_run "$standard" "$test_dir/minimal_language_$standard" \
        tests/minimal_language.cpp
    g++ -std="c++$standard" $common_flags -ffreestanding \
        -c tests/freestanding_headers.cpp \
        -o "$test_dir/freestanding_headers_$standard.o"
done

compile_and_run 11 "$test_dir/minimal_odr" \
    tests/minimal_odr_main.cpp tests/minimal_odr_other.cpp
compile_and_run 11 "$test_dir/force_generic" \
    -DSYSCAPE_FORCE_GENERIC_BACKEND=1 tests/force_generic.cpp
compile_and_run 11 "$test_dir/force_unknown" \
    -DSYSCAPE_FORCE_UNKNOWN_TARGET=1 tests/force_unknown.cpp

hosted_header_tests="
header_result
header_os
header_cpu
header_memory
header_process
header_user
header_filesystem
header_network
header_locale
header_environment
header_resource
header_power
header_storage
header_hardware
header_virtualization
header_gpu
header_display
header_security
header_sensor
header_audio
header_input
header_camera
header_bluetooth
header_wifi
header_printer
header_process_list
header_connection
header_software
header_numa
header_ipc
"

for target in $hosted_header_tests; do
    compile_and_run_hosted 17 "$test_dir/$target" "tests/$target.cpp"

    header="${target#header_}.hpp"
    probe="$test_dir/cpp11_rejection.cpp"
    log="$test_dir/cpp11_rejection.log"
    printf '#include <syscape/%s>\nint main() { return 0; }\n' "$header" > "$probe"
    if g++ -std=c++11 -Iinclude -pedantic-errors "$probe" -o \
        "$test_dir/cpp11_rejection" 2> "$log"; then
        echo "syscape/$header unexpectedly compiled as C++11" >&2
        exit 1
    fi
    if ! grep -F "syscape/$header requires C++17 or later" "$log" >/dev/null; then
        echo "syscape/$header failed as C++11 without the expected diagnostic" >&2
        cat "$log" >&2
        exit 1
    fi
done

compile_and_run_hosted 17 "$test_dir/foundation" tests/foundation.cpp
compile_and_run_hosted 17 "$test_dir/odr" tests/odr_main.cpp tests/odr_other.cpp
compile_and_run 11 "$test_dir/cmake_core_standard" tests/cmake_core_standard.cpp
compile_and_run_hosted 17 "$test_dir/cmake_hosted_standard" \
    tests/cmake_hosted_standard.cpp

for source in tests/*_force_generic.cpp; do
    target="$(basename "$source" .cpp)"
    compile_and_run_hosted 17 "$test_dir/$target" \
        -DSYSCAPE_FORCE_GENERIC_BACKEND=1 "$source"
done

for source in tests/*_haiku.cpp; do
    target="$(basename "$source" .cpp)"
    compile_and_run_hosted 17 "$test_dir/$target" "$source"
done

for source in examples/*.cpp; do
    target="$(basename "$source" .cpp)"
    compile_and_run_hosted 17 "$example_dir/$target" "$source" >/dev/null
done
