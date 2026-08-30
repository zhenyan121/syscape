function(syscape_configure_test target standard library_target)
    target_link_libraries(${target} PRIVATE ${library_target})
    set_target_properties(
        ${target}
        PROPERTIES
            CXX_STANDARD ${standard}
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO)

    if(MSVC)
        target_compile_options(
            ${target} PRIVATE /W4 /WX /permissive- /utf-8 /UNDEBUG)
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
                -Werror
                -UNDEBUG)
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
syscape_add_test(header_power 17 syscape::hosted header_power.cpp)
syscape_add_test(header_storage 17 syscape::hosted header_storage.cpp)
syscape_add_test(header_hardware 17 syscape::hosted header_hardware.cpp)
syscape_add_test(header_virtualization 17 syscape::hosted header_virtualization.cpp)
syscape_add_test(header_gpu 17 syscape::hosted header_gpu.cpp)
syscape_add_test(header_display 17 syscape::hosted header_display.cpp)
syscape_add_test(header_security 17 syscape::hosted header_security.cpp)
syscape_add_test(header_sensor 17 syscape::hosted header_sensor.cpp)
syscape_add_test(header_audio 17 syscape::hosted header_audio.cpp)
syscape_add_test(header_input 17 syscape::hosted header_input.cpp)
syscape_add_test(header_camera 17 syscape::hosted header_camera.cpp)
syscape_add_test(header_bluetooth 17 syscape::hosted header_bluetooth.cpp)
syscape_add_test(header_wifi 17 syscape::hosted header_wifi.cpp)
syscape_add_test(header_printer 17 syscape::hosted header_printer.cpp)
syscape_add_test(header_process_list 17 syscape::hosted header_process_list.cpp)
syscape_add_test(header_connection 17 syscape::hosted header_connection.cpp)
syscape_add_test(header_software 17 syscape::hosted header_software.cpp)
syscape_add_test(header_numa 17 syscape::hosted header_numa.cpp)
syscape_add_test(header_ipc 17 syscape::hosted header_ipc.cpp)
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

syscape_add_test(power_force_generic 17 syscape::hosted power_force_generic.cpp)
target_compile_definitions(
    power_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(storage_force_generic 17 syscape::hosted storage_force_generic.cpp)
target_compile_definitions(
    storage_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(hardware_force_generic 17 syscape::hosted hardware_force_generic.cpp)
target_compile_definitions(
    hardware_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(virtualization_force_generic 17 syscape::hosted virtualization_force_generic.cpp)
target_compile_definitions(
    virtualization_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(gpu_force_generic 17 syscape::hosted gpu_force_generic.cpp)
target_compile_definitions(
    gpu_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(display_force_generic 17 syscape::hosted display_force_generic.cpp)
target_compile_definitions(
    display_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(security_force_generic 17 syscape::hosted security_force_generic.cpp)
target_compile_definitions(
    security_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(sensor_force_generic 17 syscape::hosted sensor_force_generic.cpp)
target_compile_definitions(
    sensor_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(audio_force_generic 17 syscape::hosted audio_force_generic.cpp)
target_compile_definitions(
    audio_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(input_force_generic 17 syscape::hosted input_force_generic.cpp)
target_compile_definitions(
    input_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(camera_force_generic 17 syscape::hosted camera_force_generic.cpp)
target_compile_definitions(
    camera_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(bluetooth_force_generic 17 syscape::hosted bluetooth_force_generic.cpp)
target_compile_definitions(
    bluetooth_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(wifi_force_generic 17 syscape::hosted wifi_force_generic.cpp)
target_compile_definitions(
    wifi_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(printer_force_generic 17 syscape::hosted printer_force_generic.cpp)
target_compile_definitions(
    printer_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(process_list_force_generic 17 syscape::hosted process_list_force_generic.cpp)
target_compile_definitions(
    process_list_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(connection_force_generic 17 syscape::hosted connection_force_generic.cpp)
target_compile_definitions(
    connection_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(software_force_generic 17 syscape::hosted software_force_generic.cpp)
target_compile_definitions(
    software_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(numa_force_generic 17 syscape::hosted numa_force_generic.cpp)
target_compile_definitions(
    numa_force_generic
    PRIVATE SYSCAPE_FORCE_GENERIC_BACKEND=1)

syscape_add_test(ipc_force_generic 17 syscape::hosted ipc_force_generic.cpp)
target_compile_definitions(
    ipc_force_generic
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
    syscape_add_test(power_linux 17 syscape::hosted power_linux.cpp)
    syscape_add_test(storage_linux 17 syscape::hosted storage_linux.cpp)
    syscape_add_test(hardware_linux 17 syscape::hosted hardware_linux.cpp)
    syscape_add_test(virtualization_linux 17 syscape::hosted virtualization_linux.cpp)
    syscape_add_test(gpu_linux 17 syscape::hosted gpu_linux.cpp)
    syscape_add_test(display_linux 17 syscape::hosted display_linux.cpp)
    syscape_add_test(security_linux 17 syscape::hosted security_linux.cpp)
    syscape_add_test(sensor_linux 17 syscape::hosted sensor_linux.cpp)
    syscape_add_test(audio_linux 17 syscape::hosted audio_linux.cpp)
    syscape_add_test(input_linux 17 syscape::hosted input_linux.cpp)
    syscape_add_test(camera_linux 17 syscape::hosted camera_linux.cpp)
    syscape_add_test(bluetooth_linux 17 syscape::hosted bluetooth_linux.cpp)
    syscape_add_test(wifi_linux 17 syscape::hosted wifi_linux.cpp)
    syscape_add_test(printer_linux 17 syscape::hosted printer_linux.cpp)
    syscape_add_test(process_list_linux 17 syscape::hosted process_list_linux.cpp)
    syscape_add_test(connection_linux 17 syscape::hosted connection_linux.cpp)
    syscape_add_test(software_linux 17 syscape::hosted software_linux.cpp)
    syscape_add_test(numa_linux 17 syscape::hosted numa_linux.cpp)
    syscape_add_test(ipc_linux 17 syscape::hosted ipc_linux.cpp)
endif()

if(WIN32)
    syscape_add_test(
        windows_header_order
        17
        syscape::hosted
        windows_header_order.cpp)
    syscape_add_test(cpu_windows 17 syscape::hosted cpu_windows.cpp)
    syscape_add_test(memory_windows 17 syscape::hosted memory_windows.cpp)
    syscape_add_test(process_windows 17 syscape::hosted process_windows.cpp)
    syscape_add_test(user_windows 17 syscape::hosted user_windows.cpp)
    syscape_add_test(filesystem_windows 17 syscape::hosted filesystem_windows.cpp)
    syscape_add_test(network_windows 17 syscape::hosted network_windows.cpp)
    syscape_add_test(locale_windows 17 syscape::hosted locale_windows.cpp)
    syscape_add_test(environment_windows 17 syscape::hosted environment_windows.cpp)
    syscape_add_test(resource_windows 17 syscape::hosted resource_windows.cpp)
    syscape_add_test(power_windows 17 syscape::hosted power_windows.cpp)
    syscape_add_test(storage_windows 17 syscape::hosted storage_windows.cpp)
    syscape_add_test(hardware_windows 17 syscape::hosted hardware_windows.cpp)
    syscape_add_test(virtualization_windows 17 syscape::hosted virtualization_windows.cpp)
    syscape_add_test(gpu_windows 17 syscape::hosted gpu_windows.cpp)
    syscape_add_test(display_windows 17 syscape::hosted display_windows.cpp)
    syscape_add_test(security_windows 17 syscape::hosted security_windows.cpp)
    syscape_add_test(sensor_windows 17 syscape::hosted sensor_windows.cpp)
    syscape_add_test(audio_windows 17 syscape::hosted audio_windows.cpp)
    syscape_add_test(input_windows 17 syscape::hosted input_windows.cpp)
    syscape_add_test(camera_windows 17 syscape::hosted camera_windows.cpp)
    syscape_add_test(bluetooth_windows 17 syscape::hosted bluetooth_windows.cpp)
    syscape_add_test(wifi_windows 17 syscape::hosted wifi_windows.cpp)
    syscape_add_test(printer_windows 17 syscape::hosted printer_windows.cpp)
    syscape_add_test(process_list_windows 17 syscape::hosted process_list_windows.cpp)
    syscape_add_test(connection_windows 17 syscape::hosted connection_windows.cpp)
    syscape_add_test(software_windows 17 syscape::hosted software_windows.cpp)
    syscape_add_test(numa_windows 17 syscape::hosted numa_windows.cpp)
    syscape_add_test(ipc_windows 17 syscape::hosted ipc_windows.cpp)
    target_link_libraries(header_software PRIVATE advapi32 psapi)
    target_link_libraries(software_windows PRIVATE advapi32 psapi)
    target_link_libraries(odr PRIVATE advapi32 psapi)
    target_link_libraries(header_connection PRIVATE iphlpapi ws2_32)
    target_link_libraries(connection_windows PRIVATE iphlpapi ws2_32)
    target_link_libraries(odr PRIVATE ws2_32)
    target_link_libraries(header_bluetooth PRIVATE bthprops)
    target_link_libraries(bluetooth_windows PRIVATE bthprops)
    target_link_libraries(odr PRIVATE bthprops)
    target_link_libraries(header_wifi PRIVATE wlanapi)
    target_link_libraries(wifi_windows PRIVATE wlanapi)
    target_link_libraries(odr PRIVATE wlanapi)
    target_link_libraries(header_printer PRIVATE winspool)
    target_link_libraries(printer_windows PRIVATE winspool)
    target_link_libraries(odr PRIVATE winspool)
    target_link_libraries(header_audio PRIVATE ole32 uuid)
    target_link_libraries(audio_windows PRIVATE ole32 uuid)
    target_link_libraries(odr PRIVATE ole32 uuid)
    target_link_libraries(header_camera PRIVATE setupapi)
    target_link_libraries(camera_windows PRIVATE setupapi)
    target_link_libraries(header_hardware PRIVATE setupapi)
    target_link_libraries(hardware_windows PRIVATE setupapi)
    # Targets that call GetPerformanceInfo follow its documented Windows
    # linkage contract (Kernel32.lib on Windows 7 or later SDKs, otherwise
    # Psapi.lib) without imposing it on unrelated Hosted Full consumers.
    target_link_libraries(header_memory PRIVATE psapi)
    target_link_libraries(memory_windows PRIVATE psapi)
    target_link_libraries(resource_windows PRIVATE psapi)
    # Targets that call the CPU power information API follow its documented
    # Windows linkage contract without imposing PowrProf on unrelated Hosted
    # Full consumers.
    target_link_libraries(cpu_windows PRIVATE powrprof)
    # Targets that call the network API follow its documented Windows linkage
    # contract without imposing Iphlpapi on unrelated Hosted Full consumers.
    target_link_libraries(odr PRIVATE iphlpapi)
    target_link_libraries(header_network PRIVATE ws2_32)
    target_link_libraries(network_windows PRIVATE iphlpapi ws2_32)
    # Targets that call the user privilege and session APIs follow their documented
    # Windows linkage contracts without imposing Advapi32 or Wtsapi32 on unrelated consumers.
    target_link_libraries(header_user PRIVATE advapi32 wtsapi32)
    target_link_libraries(user_windows PRIVATE advapi32 wtsapi32)
    target_link_libraries(odr PRIVATE advapi32 wtsapi32)
    # Storage, hardware, and camera enumeration use documented SetupAPI APIs.
    target_link_libraries(header_storage PRIVATE setupapi)
    target_link_libraries(storage_windows PRIVATE setupapi)
    target_link_libraries(odr PRIVATE setupapi)
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
    syscape_add_test(power_macos 17 syscape::hosted power_macos.cpp)
    syscape_add_test(storage_macos 17 syscape::hosted storage_macos.cpp)
    syscape_add_test(hardware_macos 17 syscape::hosted hardware_macos.cpp)
    syscape_add_test(virtualization_macos 17 syscape::hosted virtualization_macos.cpp)
    syscape_add_test(gpu_macos 17 syscape::hosted gpu_macos.cpp)
    syscape_add_test(display_macos 17 syscape::hosted display_macos.cpp)
    syscape_add_test(security_macos 17 syscape::hosted security_macos.cpp)
    syscape_add_test(sensor_macos 17 syscape::hosted sensor_macos.cpp)
    syscape_add_test(audio_macos 17 syscape::hosted audio_macos.cpp)
    syscape_add_test(input_macos 17 syscape::hosted input_macos.cpp)
    syscape_add_test(camera_macos 17 syscape::hosted camera_macos.cpp)
    syscape_add_test(bluetooth_macos 17 syscape::hosted bluetooth_macos.cpp)
    syscape_add_test(wifi_macos 17 syscape::hosted wifi_macos.cpp)
    syscape_add_test(printer_macos 17 syscape::hosted printer_macos.cpp)
    syscape_add_test(process_list_macos 17 syscape::hosted process_list_macos.cpp)
    syscape_add_test(connection_macos 17 syscape::hosted connection_macos.cpp)
    syscape_add_test(software_macos 17 syscape::hosted software_macos.cpp)
    syscape_add_test(numa_macos 17 syscape::hosted numa_macos.cpp)
    syscape_add_test(ipc_macos 17 syscape::hosted ipc_macos.cpp)
    target_link_libraries(
        header_software PRIVATE "-framework CoreFoundation")
    target_link_libraries(
        software_macos PRIVATE "-framework CoreFoundation")
    target_link_libraries(
        header_virtualization PRIVATE "-framework IOKit" "-framework CoreFoundation")
    target_link_libraries(
        virtualization_macos PRIVATE "-framework IOKit" "-framework CoreFoundation")
    target_link_libraries(
        header_bluetooth PRIVATE "-framework IOKit" "-framework CoreFoundation")
    target_link_libraries(
        bluetooth_macos PRIVATE "-framework IOKit" "-framework CoreFoundation")
    # Targets that call the IOKit power-sources API follow its documented
    # Apple linkage contract without imposing the frameworks on unrelated
    # Hosted Full consumers.
    target_link_libraries(
        header_power PRIVATE "-framework IOKit" "-framework CoreFoundation")
    target_link_libraries(
        power_macos PRIVATE "-framework IOKit" "-framework CoreFoundation")
    target_link_libraries(
        odr PRIVATE "-framework IOKit" "-framework CoreFoundation")
    # Targets that call the platform-expert hardware identity interfaces
    # follow the documented Apple linkage contract without imposing the
    # frameworks on unrelated Hosted Full consumers.
    target_link_libraries(
        header_hardware PRIVATE "-framework IOKit" "-framework CoreFoundation")
    target_link_libraries(
        hardware_macos PRIVATE "-framework IOKit" "-framework CoreFoundation")
    # Targets that call the DiskArbitration storage interfaces follow the
    # documented Apple linkage contract without imposing the frameworks on
    # unrelated Hosted Full consumers.
    target_link_libraries(
        header_storage PRIVATE "-framework IOKit" "-framework CoreFoundation"
        "-framework DiskArbitration")
    target_link_libraries(
        storage_macos PRIVATE "-framework IOKit" "-framework CoreFoundation"
        "-framework DiskArbitration")
    target_link_libraries(
        odr PRIVATE "-framework IOKit" "-framework CoreFoundation"
        "-framework DiskArbitration")
    # Targets that call the SystemConfiguration DNS interface follow its
    # documented Apple linkage contract without imposing the frameworks on
    # unrelated Hosted Full consumers.
    target_link_libraries(
        header_network PRIVATE "-framework CoreFoundation"
        "-framework SystemConfiguration")
    target_link_libraries(
        network_macos PRIVATE "-framework CoreFoundation"
        "-framework SystemConfiguration")
    target_link_libraries(
        odr PRIVATE "-framework CoreFoundation"
        "-framework SystemConfiguration")
    # Targets that call the CoreFoundation locale-preference and time-zone
    # interfaces follow their documented Apple linkage contract without
    # imposing the framework on unrelated Hosted Full consumers.
    target_link_libraries(
        header_locale PRIVATE "-framework CoreFoundation")
    target_link_libraries(
        locale_macos PRIVATE "-framework CoreFoundation")
    # Targets that call the IOKit graphics interfaces follow the documented
    # Apple linkage contract without imposing the frameworks on unrelated
    # Hosted Full consumers.
    target_link_libraries(
        header_gpu PRIVATE "-framework IOKit" "-framework CoreFoundation")
    target_link_libraries(
        gpu_macos PRIVATE "-framework IOKit" "-framework CoreFoundation")
    # Targets that call CoreGraphics display interfaces follow the documented
    # Apple linkage contract without imposing the frameworks on unrelated
    # Hosted Full consumers.
    target_link_libraries(
        header_display PRIVATE "-framework CoreGraphics" "-framework CoreFoundation")
    target_link_libraries(
        display_macos PRIVATE "-framework CoreGraphics" "-framework CoreFoundation")
    target_link_libraries(
        odr PRIVATE "-framework CoreGraphics")
    # Targets that call CoreAudio interfaces follow the documented Apple
    # linkage contract without imposing the frameworks on unrelated Hosted Full consumers.
    target_link_libraries(
        header_audio PRIVATE "-framework CoreAudio" "-framework CoreFoundation")
    target_link_libraries(
        audio_macos PRIVATE "-framework CoreAudio" "-framework CoreFoundation")
    # Targets that call IOKit HID interfaces follow the documented Apple
    # linkage contract without imposing the frameworks on unrelated Hosted Full consumers.
    target_link_libraries(
        header_input PRIVATE "-framework IOKit" "-framework CoreFoundation")
    target_link_libraries(
        input_macos PRIVATE "-framework IOKit" "-framework CoreFoundation")
    # Targets that call CoreMediaIO camera interfaces follow the documented Apple
    # linkage contract without imposing the frameworks on unrelated Hosted Full consumers.
    target_link_libraries(
        header_camera PRIVATE "-framework CoreMediaIO" "-framework CoreFoundation")
    target_link_libraries(
        camera_macos PRIVATE "-framework CoreMediaIO" "-framework CoreFoundation")
    target_link_libraries(
        odr PRIVATE "-framework CoreAudio" "-framework IOKit" "-framework CoreMediaIO")
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
syscape_expect_cpp11_rejection(
    "syscape/power.hpp"
    "syscape/power.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/storage.hpp"
    "syscape/storage.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/hardware.hpp"
    "syscape/hardware.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/virtualization.hpp"
    "syscape/virtualization.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/gpu.hpp"
    "syscape/gpu.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/display.hpp"
    "syscape/display.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/security.hpp"
    "syscape/security.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/sensor.hpp"
    "syscape/sensor.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/audio.hpp"
    "syscape/audio.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/input.hpp"
    "syscape/input.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/camera.hpp"
    "syscape/camera.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/bluetooth.hpp"
    "syscape/bluetooth.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/wifi.hpp"
    "syscape/wifi.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/printer.hpp"
    "syscape/printer.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/process_list.hpp"
    "syscape/process_list.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/connection.hpp"
    "syscape/connection.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/software.hpp"
    "syscape/software.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/numa.hpp"
    "syscape/numa.hpp requires C++17 or later")
syscape_expect_cpp11_rejection(
    "syscape/ipc.hpp"
    "syscape/ipc.hpp requires C++17 or later")
