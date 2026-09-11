foreach(configuration IN ITEMS RELEASE RELWITHDEBINFO MINSIZEREL)
    string(
        REGEX REPLACE
        "(^| )[/-]DNDEBUG( |$)"
        " "
        CMAKE_CXX_FLAGS_${configuration}
        "${CMAKE_CXX_FLAGS_${configuration}}")
endforeach()

function(syscape_configure_test target standard library_target)
    target_link_libraries(${target} PRIVATE ${library_target})
    set_target_properties(
        ${target}
        PROPERTIES
            CXX_STANDARD ${standard}
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO)

    # Ubuntu's packaged Emscripten can emit Node.js launchers that use the
    # global fetch implementation for local .wasm paths.  Modern Node.js
    # rejects those paths as invalid URLs, so load test binaries synchronously.
    if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten" OR EMSCRIPTEN)
        target_link_options(${target} PRIVATE -sWASM_ASYNC_COMPILATION=0)
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8)
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
    if(NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
        add_test(NAME ${target} COMMAND ${target})
    endif()
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

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
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

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    syscape_add_test(os_freebsd 17 syscape::hosted os_freebsd.cpp)
    syscape_add_test(cpu_freebsd 17 syscape::hosted cpu_freebsd.cpp)
    syscape_add_test(memory_freebsd 17 syscape::hosted memory_freebsd.cpp)
    syscape_add_test(process_freebsd 17 syscape::hosted process_freebsd.cpp)
    syscape_add_test(user_freebsd 17 syscape::hosted user_freebsd.cpp)
    syscape_add_test(filesystem_freebsd 17 syscape::hosted filesystem_freebsd.cpp)
    syscape_add_test(network_freebsd 17 syscape::hosted network_freebsd.cpp)
    syscape_add_test(locale_freebsd 17 syscape::hosted locale_freebsd.cpp)
    syscape_add_test(environment_freebsd 17 syscape::hosted environment_freebsd.cpp)
    syscape_add_test(resource_freebsd 17 syscape::hosted resource_freebsd.cpp)
    syscape_add_test(virtualization_freebsd 17 syscape::hosted virtualization_freebsd.cpp)
    syscape_add_test(hardware_freebsd 17 syscape::hosted hardware_freebsd.cpp)
    syscape_add_test(power_freebsd 17 syscape::hosted power_freebsd.cpp)
    syscape_add_test(sensor_freebsd 17 syscape::hosted sensor_freebsd.cpp)
    syscape_add_test(security_freebsd 17 syscape::hosted security_freebsd.cpp)
    syscape_add_test(numa_freebsd 17 syscape::hosted numa_freebsd.cpp)
    syscape_add_test(process_list_freebsd 17 syscape::hosted process_list_freebsd.cpp)
    syscape_add_test(printer_freebsd 17 syscape::hosted printer_freebsd.cpp)
    syscape_add_test(audio_freebsd 17 syscape::hosted audio_freebsd.cpp)
    syscape_add_test(software_freebsd 17 syscape::hosted software_freebsd.cpp)
    syscape_add_test(storage_freebsd 17 syscape::hosted storage_freebsd.cpp)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
    syscape_add_test(os_openbsd 17 syscape::hosted os_openbsd.cpp)
    syscape_add_test(cpu_openbsd 17 syscape::hosted cpu_openbsd.cpp)
    syscape_add_test(memory_openbsd 17 syscape::hosted memory_openbsd.cpp)
    syscape_add_test(process_openbsd 17 syscape::hosted process_openbsd.cpp)
    syscape_add_test(user_openbsd 17 syscape::hosted user_openbsd.cpp)
    syscape_add_test(filesystem_openbsd 17 syscape::hosted filesystem_openbsd.cpp)
    syscape_add_test(network_openbsd 17 syscape::hosted network_openbsd.cpp)
    syscape_add_test(locale_openbsd 17 syscape::hosted locale_openbsd.cpp)
    syscape_add_test(environment_openbsd 17 syscape::hosted environment_openbsd.cpp)
    syscape_add_test(resource_openbsd 17 syscape::hosted resource_openbsd.cpp)
    syscape_add_test(virtualization_openbsd 17 syscape::hosted virtualization_openbsd.cpp)
    syscape_add_test(hardware_openbsd 17 syscape::hosted hardware_openbsd.cpp)
    syscape_add_test(power_openbsd 17 syscape::hosted power_openbsd.cpp)
    syscape_add_test(sensor_openbsd 17 syscape::hosted sensor_openbsd.cpp)
    syscape_add_test(security_openbsd 17 syscape::hosted security_openbsd.cpp)
    syscape_add_test(numa_openbsd 17 syscape::hosted numa_openbsd.cpp)
    syscape_add_test(process_list_openbsd 17 syscape::hosted process_list_openbsd.cpp)
    syscape_add_test(printer_openbsd 17 syscape::hosted printer_openbsd.cpp)
    syscape_add_test(audio_openbsd 17 syscape::hosted audio_openbsd.cpp)
    syscape_add_test(software_openbsd 17 syscape::hosted software_openbsd.cpp)
    syscape_add_test(storage_openbsd 17 syscape::hosted storage_openbsd.cpp)
    syscape_add_test(ipc_openbsd 17 syscape::hosted ipc_openbsd.cpp)
    syscape_add_test(connection_openbsd 17 syscape::hosted connection_openbsd.cpp)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
    syscape_add_test(os_netbsd 17 syscape::hosted os_netbsd.cpp)
    syscape_add_test(cpu_netbsd 17 syscape::hosted cpu_netbsd.cpp)
    syscape_add_test(memory_netbsd 17 syscape::hosted memory_netbsd.cpp)
    syscape_add_test(process_netbsd 17 syscape::hosted process_netbsd.cpp)
    syscape_add_test(user_netbsd 17 syscape::hosted user_netbsd.cpp)
    syscape_add_test(filesystem_netbsd 17 syscape::hosted filesystem_netbsd.cpp)
    syscape_add_test(network_netbsd 17 syscape::hosted network_netbsd.cpp)
    syscape_add_test(locale_netbsd 17 syscape::hosted locale_netbsd.cpp)
    syscape_add_test(environment_netbsd 17 syscape::hosted environment_netbsd.cpp)
    syscape_add_test(resource_netbsd 17 syscape::hosted resource_netbsd.cpp)
    syscape_add_test(virtualization_netbsd 17 syscape::hosted virtualization_netbsd.cpp)
    syscape_add_test(hardware_netbsd 17 syscape::hosted hardware_netbsd.cpp)
    syscape_add_test(power_netbsd 17 syscape::hosted power_netbsd.cpp)
    syscape_add_test(sensor_netbsd 17 syscape::hosted sensor_netbsd.cpp)
    syscape_add_test(security_netbsd 17 syscape::hosted security_netbsd.cpp)
    syscape_add_test(numa_netbsd 17 syscape::hosted numa_netbsd.cpp)
    syscape_add_test(process_list_netbsd 17 syscape::hosted process_list_netbsd.cpp)
    syscape_add_test(printer_netbsd 17 syscape::hosted printer_netbsd.cpp)
    syscape_add_test(audio_netbsd 17 syscape::hosted audio_netbsd.cpp)
    syscape_add_test(software_netbsd 17 syscape::hosted software_netbsd.cpp)
    syscape_add_test(storage_netbsd 17 syscape::hosted storage_netbsd.cpp)
    syscape_add_test(ipc_netbsd 17 syscape::hosted ipc_netbsd.cpp)
    syscape_add_test(connection_netbsd 17 syscape::hosted connection_netbsd.cpp)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "DragonFly")
    syscape_add_test(os_dragonfly 17 syscape::hosted os_dragonfly.cpp)
    syscape_add_test(cpu_dragonfly 17 syscape::hosted cpu_dragonfly.cpp)
    syscape_add_test(memory_dragonfly 17 syscape::hosted memory_dragonfly.cpp)
    syscape_add_test(process_dragonfly 17 syscape::hosted process_dragonfly.cpp)
    syscape_add_test(user_dragonfly 17 syscape::hosted user_dragonfly.cpp)
    syscape_add_test(filesystem_dragonfly 17 syscape::hosted filesystem_dragonfly.cpp)
    syscape_add_test(network_dragonfly 17 syscape::hosted network_dragonfly.cpp)
    syscape_add_test(locale_dragonfly 17 syscape::hosted locale_dragonfly.cpp)
    syscape_add_test(environment_dragonfly 17 syscape::hosted environment_dragonfly.cpp)
    syscape_add_test(resource_dragonfly 17 syscape::hosted resource_dragonfly.cpp)
    syscape_add_test(virtualization_dragonfly 17 syscape::hosted virtualization_dragonfly.cpp)
    syscape_add_test(hardware_dragonfly 17 syscape::hosted hardware_dragonfly.cpp)
    syscape_add_test(power_dragonfly 17 syscape::hosted power_dragonfly.cpp)
    syscape_add_test(sensor_dragonfly 17 syscape::hosted sensor_dragonfly.cpp)
    syscape_add_test(security_dragonfly 17 syscape::hosted security_dragonfly.cpp)
    syscape_add_test(numa_dragonfly 17 syscape::hosted numa_dragonfly.cpp)
    syscape_add_test(process_list_dragonfly 17 syscape::hosted process_list_dragonfly.cpp)
    syscape_add_test(printer_dragonfly 17 syscape::hosted printer_dragonfly.cpp)
    syscape_add_test(audio_dragonfly 17 syscape::hosted audio_dragonfly.cpp)
    syscape_add_test(software_dragonfly 17 syscape::hosted software_dragonfly.cpp)
    syscape_add_test(storage_dragonfly 17 syscape::hosted storage_dragonfly.cpp)
    syscape_add_test(ipc_dragonfly 17 syscape::hosted ipc_dragonfly.cpp)
    syscape_add_test(connection_dragonfly 17 syscape::hosted connection_dragonfly.cpp)
endif()

if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    syscape_add_test(os_android 17 syscape::hosted os_android.cpp)
    syscape_add_test(cpu_android 17 syscape::hosted cpu_android.cpp)
    syscape_add_test(memory_android 17 syscape::hosted memory_android.cpp)
    syscape_add_test(process_android 17 syscape::hosted process_android.cpp)
    syscape_add_test(user_android 17 syscape::hosted user_android.cpp)
    syscape_add_test(filesystem_android 17 syscape::hosted filesystem_android.cpp)
    syscape_add_test(network_android 17 syscape::hosted network_android.cpp)
    syscape_add_test(locale_android 17 syscape::hosted locale_android.cpp)
    syscape_add_test(environment_android 17 syscape::hosted environment_android.cpp)
    syscape_add_test(resource_android 17 syscape::hosted resource_android.cpp)
    syscape_add_test(virtualization_android 17 syscape::hosted virtualization_android.cpp)
    syscape_add_test(hardware_android 17 syscape::hosted hardware_android.cpp)
    syscape_add_test(power_android 17 syscape::hosted power_android.cpp)
    syscape_add_test(sensor_android 17 syscape::hosted sensor_android.cpp)
    syscape_add_test(security_android 17 syscape::hosted security_android.cpp)
    syscape_add_test(numa_android 17 syscape::hosted numa_android.cpp)
    syscape_add_test(process_list_android 17 syscape::hosted process_list_android.cpp)
    syscape_add_test(printer_android 17 syscape::hosted printer_android.cpp)
    syscape_add_test(audio_android 17 syscape::hosted audio_android.cpp)
    syscape_add_test(software_android 17 syscape::hosted software_android.cpp)
    syscape_add_test(storage_android 17 syscape::hosted storage_android.cpp)
    syscape_add_test(ipc_android 17 syscape::hosted ipc_android.cpp)
    syscape_add_test(connection_android 17 syscape::hosted connection_android.cpp)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "^(OpenHarmony|OHOS)$" OR OHOS)
    set(OHOS_TEST_TARGETS
        os_openharmony
        cpu_openharmony
        memory_openharmony
        process_openharmony
        user_openharmony
        filesystem_openharmony
        network_openharmony
        locale_openharmony
        environment_openharmony
        resource_openharmony
        virtualization_openharmony
        hardware_openharmony
        power_openharmony
        sensor_openharmony
        security_openharmony
        numa_openharmony
        process_list_openharmony
        printer_openharmony
        audio_openharmony
        software_openharmony
        storage_openharmony
        ipc_openharmony
        connection_openharmony)
    foreach(ohos_target IN LISTS OHOS_TEST_TARGETS)
        syscape_add_test(${ohos_target} 17 syscape::hosted ${ohos_target}.cpp)
        target_compile_definitions(${ohos_target} PRIVATE __OHOS__=1)
        if(CMAKE_DL_LIBS)
            target_link_libraries(${ohos_target} PRIVATE ${CMAKE_DL_LIBS})
        endif()
    endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    syscape_add_test(os_solaris 17 syscape::hosted os_solaris.cpp)
    syscape_add_test(cpu_solaris 17 syscape::hosted cpu_solaris.cpp)
    syscape_add_test(memory_solaris 17 syscape::hosted memory_solaris.cpp)
    syscape_add_test(process_solaris 17 syscape::hosted process_solaris.cpp)
    syscape_add_test(user_solaris 17 syscape::hosted user_solaris.cpp)
    syscape_add_test(filesystem_solaris 17 syscape::hosted filesystem_solaris.cpp)
    syscape_add_test(network_solaris 17 syscape::hosted network_solaris.cpp)
    syscape_add_test(locale_solaris 17 syscape::hosted locale_solaris.cpp)
    syscape_add_test(environment_solaris 17 syscape::hosted environment_solaris.cpp)
    syscape_add_test(resource_solaris 17 syscape::hosted resource_solaris.cpp)
    syscape_add_test(virtualization_solaris 17 syscape::hosted virtualization_solaris.cpp)
    syscape_add_test(hardware_solaris 17 syscape::hosted hardware_solaris.cpp)
    syscape_add_test(power_solaris 17 syscape::hosted power_solaris.cpp)
    syscape_add_test(sensor_solaris 17 syscape::hosted sensor_solaris.cpp)
    syscape_add_test(security_solaris 17 syscape::hosted security_solaris.cpp)
    syscape_add_test(numa_solaris 17 syscape::hosted numa_solaris.cpp)
    syscape_add_test(process_list_solaris 17 syscape::hosted process_list_solaris.cpp)
    syscape_add_test(printer_solaris 17 syscape::hosted printer_solaris.cpp)
    syscape_add_test(audio_solaris 17 syscape::hosted audio_solaris.cpp)
    syscape_add_test(software_solaris 17 syscape::hosted software_solaris.cpp)
    syscape_add_test(storage_solaris 17 syscape::hosted storage_solaris.cpp)
    syscape_add_test(ipc_solaris 17 syscape::hosted ipc_solaris.cpp)
    syscape_add_test(connection_solaris 17 syscape::hosted connection_solaris.cpp)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    set(HAIKU_TEST_TARGETS
        error_haiku
        os_haiku
        cpu_haiku
        memory_haiku
        process_haiku
        user_haiku
        filesystem_haiku
        network_haiku
        locale_haiku
        environment_haiku
        resource_haiku
        virtualization_haiku
        hardware_haiku
        power_haiku
        sensor_haiku
        security_haiku
        numa_haiku
        process_list_haiku
        printer_haiku
        audio_haiku
        software_haiku
        storage_haiku
        ipc_haiku
        connection_haiku)
    foreach(haiku_target IN LISTS HAIKU_TEST_TARGETS)
        syscape_add_test(${haiku_target} 17 syscape::hosted ${haiku_target}.cpp)
        target_link_libraries(${haiku_target} PRIVATE network)
    endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "AIX" OR AIX)
    set(AIX_TEST_TARGETS
        os_aix
        cpu_aix
        memory_aix
        process_aix
        user_aix
        filesystem_aix
        network_aix
        locale_aix
        environment_aix
        resource_aix
        virtualization_aix
        hardware_aix
        power_aix
        sensor_aix
        security_aix
        numa_aix
        process_list_aix
        printer_aix
        audio_aix
        software_aix
        storage_aix
        ipc_aix
        connection_aix)
    foreach(aix_target IN LISTS AIX_TEST_TARGETS)
        syscape_add_test(${aix_target} 17 syscape::hosted ${aix_target}.cpp)
        target_compile_definitions(${aix_target} PRIVATE _AIX=1 _BSD=44)
        if(CMAKE_SYSTEM_NAME STREQUAL "AIX")
            target_link_libraries(${aix_target} PRIVATE perfstat bsd)
        endif()
    endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "HP-UX" OR HPUX)
    set(HPUX_TEST_TARGETS
        os_hpux
        cpu_hpux
        memory_hpux
        process_hpux
        user_hpux
        filesystem_hpux
        network_hpux
        locale_hpux
        environment_hpux
        resource_hpux
        virtualization_hpux
        hardware_hpux
        power_hpux
        sensor_hpux
        security_hpux
        numa_hpux
        process_list_hpux
        printer_hpux
        audio_hpux
        software_hpux
        storage_hpux
        ipc_hpux
        connection_hpux)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "HP-UX")
        add_library(hpux_pstat_compat STATIC compat/hpux/pstat_compat.cpp)
        target_include_directories(hpux_pstat_compat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/compat/hpux)
        target_compile_features(hpux_pstat_compat PUBLIC cxx_std_17)
        target_compile_definitions(hpux_pstat_compat PUBLIC _PSTAT64=1 _ICOD_BASE_INFO=1)
    endif()
    foreach(hpux_target IN LISTS HPUX_TEST_TARGETS)
        syscape_add_test(${hpux_target} 17 syscape::hosted ${hpux_target}.cpp)
        target_compile_definitions(${hpux_target} PRIVATE __hpux=1 _XOPEN_SOURCE_EXTENDED=1 _PSTAT64=1 _ICOD_BASE_INFO=1)
        if(NOT CMAKE_SYSTEM_NAME STREQUAL "HP-UX")
            target_link_libraries(${hpux_target} PRIVATE hpux_pstat_compat)
            target_include_directories(${hpux_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compat/hpux)
            target_compile_definitions(${hpux_target} PRIVATE SYSCAPE_HPUX_PSTAT_MOCK=1)
        endif()
    endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "GNU" OR HURD)
    set(HURD_TEST_TARGETS
        os_hurd
        cpu_hurd
        memory_hurd
        process_hurd
        user_hurd
        filesystem_hurd
        network_hurd
        locale_hurd
        environment_hurd
        resource_hurd
        virtualization_hurd
        hardware_hurd
        power_hurd
        sensor_hurd
        security_hurd
        numa_hurd
        process_list_hurd
        printer_hurd
        audio_hurd
        software_hurd
        storage_hurd
        ipc_hurd
        connection_hurd)
    foreach(hurd_target IN LISTS HURD_TEST_TARGETS)
        syscape_add_test(${hurd_target} 17 syscape::hosted ${hurd_target}.cpp)
        target_compile_definitions(${hurd_target} PRIVATE __GNU__=1 HURD=1)
    endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "SerenityOS" OR SERENITY)
    set(SERENITY_TEST_TARGETS
        os_serenity
        cpu_serenity
        memory_serenity
        process_serenity
        user_serenity
        filesystem_serenity
        network_serenity
        locale_serenity
        environment_serenity
        resource_serenity
        virtualization_serenity
        hardware_serenity
        power_serenity
        sensor_serenity
        security_serenity
        numa_serenity
        process_list_serenity
        printer_serenity
        audio_serenity
        software_serenity
        storage_serenity
        ipc_serenity
        connection_serenity)
    foreach(serenity_target IN LISTS SERENITY_TEST_TARGETS)
        syscape_add_test(${serenity_target} 17 syscape::hosted ${serenity_target}.cpp)
        target_compile_definitions(${serenity_target} PRIVATE __serenity__=1 SERENITY=1)
    endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Redox" OR REDOX)
    set(REDOX_TEST_TARGETS
        os_redox
        cpu_redox
        memory_redox
        process_redox
        user_redox
        filesystem_redox
        network_redox
        locale_redox
        environment_redox
        resource_redox
        virtualization_redox
        hardware_redox
        power_redox
        sensor_redox
        security_redox
        numa_redox
        process_list_redox
        printer_redox
        audio_redox
        software_redox
        storage_redox
        ipc_redox
        connection_redox)
    foreach(redox_target IN LISTS REDOX_TEST_TARGETS)
        syscape_add_test(${redox_target} 17 syscape::hosted ${redox_target}.cpp)
        target_compile_definitions(${redox_target} PRIVATE __redox__=1 REDOX=1)
    endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Minix" OR MINIX)
    set(MINIX_TEST_TARGETS
        os_minix
        cpu_minix
        memory_minix
        process_minix
        user_minix
        filesystem_minix
        network_minix
        locale_minix
        environment_minix
        resource_minix
        virtualization_minix
        hardware_minix
        power_minix
        sensor_minix
        security_minix
        numa_minix
        process_list_minix
        printer_minix
        audio_minix
        software_minix
        storage_minix
        ipc_minix
        connection_minix)
    foreach(minix_target IN LISTS MINIX_TEST_TARGETS)
        syscape_add_test(${minix_target} 17 syscape::hosted ${minix_target}.cpp)
        target_compile_definitions(${minix_target} PRIVATE __minix=1 MINIX=1)
    endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "QNX" OR QNX)
    set(QNX_TEST_TARGETS
        os_qnx
        cpu_qnx
        memory_qnx
        process_qnx
        user_qnx
        filesystem_qnx
        network_qnx
        locale_qnx
        environment_qnx
        resource_qnx
        virtualization_qnx
        hardware_qnx
        power_qnx
        sensor_qnx
        security_qnx
        numa_qnx
        process_list_qnx
        printer_qnx
        audio_qnx
        software_qnx
        storage_qnx
        ipc_qnx
        connection_qnx)
    foreach(qnx_target IN LISTS QNX_TEST_TARGETS)
        syscape_add_test(${qnx_target} 17 syscape::hosted ${qnx_target}.cpp)
        target_compile_definitions(${qnx_target} PRIVATE __QNXNTO__=1 __QNX__=1 QNX=1)
    endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "VxWorks" OR VXWORKS)
    set(VXWORKS_TEST_TARGETS
        os_vxworks
        cpu_vxworks
        memory_vxworks
        process_vxworks
        user_vxworks
        filesystem_vxworks
        network_vxworks
        locale_vxworks
        environment_vxworks
        resource_vxworks
        virtualization_vxworks
        hardware_vxworks
        power_vxworks
        sensor_vxworks
        security_vxworks
        numa_vxworks
        process_list_vxworks
        printer_vxworks
        audio_vxworks
        software_vxworks
        storage_vxworks
        ipc_vxworks
        connection_vxworks)
    foreach(vxworks_target IN LISTS VXWORKS_TEST_TARGETS)
        syscape_add_test(${vxworks_target} 17 syscape::hosted ${vxworks_target}.cpp)
        target_compile_definitions(${vxworks_target} PRIVATE __VXWORKS__=1 VXWORKS=1 __RTP__=1)
    endforeach()
    target_compile_definitions(network_vxworks PRIVATE SYSCAPE_HAS_VXWORKS_GETIFADDRS=1)
    syscape_add_test(kernel_vxworks 17 syscape::hosted kernel_vxworks.cpp)
    target_compile_definitions(kernel_vxworks PRIVATE __VXWORKS__=1 VXWORKS=1 _WRS_KERNEL=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "RTEMS" OR RTEMS)
    set(RTEMS_TEST_TARGETS
        os_rtems
        cpu_rtems
        memory_rtems
        process_rtems
        user_rtems
        filesystem_rtems
        network_rtems
        locale_rtems
        environment_rtems
        resource_rtems
        virtualization_rtems
        hardware_rtems
        power_rtems
        sensor_rtems
        security_rtems
        numa_rtems
        process_list_rtems
        printer_rtems
        audio_rtems
        software_rtems
        storage_rtems
        ipc_rtems
        connection_rtems)
    foreach(rtems_target IN LISTS RTEMS_TEST_TARGETS)
        syscape_add_test(${rtems_target} 17 syscape::hosted ${rtems_target}.cpp)
        target_compile_definitions(${rtems_target} PRIVATE __rtems__=1 RTEMS=1)
    endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Zephyr" OR ZEPHYR)
    set(ZEPHYR_TEST_TARGETS
        os_zephyr
        cpu_zephyr
        memory_zephyr
        process_zephyr
        user_zephyr
        filesystem_zephyr
        network_zephyr
        locale_zephyr
        environment_zephyr
        resource_zephyr
        virtualization_zephyr
        hardware_zephyr
        power_zephyr
        sensor_zephyr
        security_zephyr
        numa_zephyr
        process_list_zephyr
        printer_zephyr
        audio_zephyr
        software_zephyr
        storage_zephyr
        ipc_zephyr
        connection_zephyr)
    foreach(zephyr_target IN LISTS ZEPHYR_TEST_TARGETS)
        syscape_add_test(${zephyr_target} 17 syscape::hosted ${zephyr_target}.cpp)
        target_compile_definitions(${zephyr_target} PRIVATE __ZEPHYR__=1 ZEPHYR=1)
    endforeach()

    syscape_add_test(posix_zephyr 17 syscape::hosted posix_zephyr.cpp)
    target_compile_definitions(
        posix_zephyr
        PRIVATE
            __ZEPHYR__=1
            ZEPHYR=1
            CONFIG_POSIX_SINGLE_PROCESS=1
            CONFIG_POSIX_TIMERS=1)

    syscape_add_test(macro_zephyr 17 syscape::hosted macro_zephyr.cpp)
    target_compile_definitions(
        macro_zephyr
        PRIVATE
            __ZEPHYR__=1
            ZEPHYR=1
            CONFIG_POSIX_SINGLE_PROCESS=1
            CONFIG_POSIX_TIMERS=1
            CONFIG_POSIX_SYSCONF_IMPL_MACRO=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "NuttX" OR NUTTX)
    set(NUTTX_TEST_TARGETS
        os_nuttx
        cpu_nuttx
        memory_nuttx
        process_nuttx
        user_nuttx
        filesystem_nuttx
        network_nuttx
        locale_nuttx
        environment_nuttx
        resource_nuttx
        virtualization_nuttx
        hardware_nuttx
        power_nuttx
        sensor_nuttx
        security_nuttx
        numa_nuttx
        process_list_nuttx
        printer_nuttx
        audio_nuttx
        software_nuttx
        storage_nuttx
        ipc_nuttx
        connection_nuttx)
    foreach(nuttx_target IN LISTS NUTTX_TEST_TARGETS)
        syscape_add_test(${nuttx_target} 17 syscape::hosted ${nuttx_target}.cpp)
        target_compile_definitions(${nuttx_target} PRIVATE NUTTX=1)
    endforeach()

    foreach(nuttx_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection)
        set(nuttx_header_target "header_${nuttx_domain}_nuttx")
        syscape_add_test(
            ${nuttx_header_target}
            17
            syscape::hosted
            header_${nuttx_domain}.cpp)
        target_compile_definitions(${nuttx_header_target} PRIVATE NUTTX=1)
    endforeach()

    syscape_add_test(
        nuttx_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(nuttx_odr PRIVATE NUTTX=1)

    syscape_add_test(nuttx_detection 11 syscape::syscape nuttx_detection.cpp)
    target_compile_definitions(nuttx_detection PRIVATE __NuttX__=1)

    syscape_add_test(disabled_nuttx 17 syscape::hosted disabled_nuttx.cpp)
    target_compile_definitions(
        disabled_nuttx
        PRIVATE
            __NuttX__=1
            CONFIG_DISABLE_ENVIRON=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "WASI" OR WASI)
    set(WASI_TEST_TARGETS
        os_wasi
        cpu_wasi
        memory_wasi
        process_wasi
        user_wasi
        filesystem_wasi
        network_wasi
        locale_wasi
        environment_wasi
        resource_wasi
        virtualization_wasi
        hardware_wasi
        power_wasi
        sensor_wasi
        security_wasi
        numa_wasi
        process_list_wasi
        printer_wasi
        audio_wasi
        software_wasi
        storage_wasi
        ipc_wasi
        connection_wasi)
    foreach(wasi_target IN LISTS WASI_TEST_TARGETS)
        syscape_add_test(${wasi_target} 17 syscape::hosted ${wasi_target}.cpp)
        target_compile_definitions(${wasi_target} PRIVATE WASI=1 __wasi__=1)
    endforeach()

    foreach(wasi_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection)
        set(wasi_header_target "header_${wasi_domain}_wasi")
        syscape_add_test(
            ${wasi_header_target}
            17
            syscape::hosted
            header_${wasi_domain}.cpp)
        target_compile_definitions(${wasi_header_target} PRIVATE WASI=1 __wasi__=1)
    endforeach()

    syscape_add_test(
        wasi_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(wasi_odr PRIVATE WASI=1 __wasi__=1)

    syscape_add_test(wasi_detection 11 syscape::syscape wasi_detection.cpp)
    target_compile_definitions(wasi_detection PRIVATE WASI=1 __wasi__=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten" OR EMSCRIPTEN)
    set(EMSCRIPTEN_TEST_TARGETS
        os_emscripten
        cpu_emscripten
        memory_emscripten
        process_emscripten
        user_emscripten
        filesystem_emscripten
        network_emscripten
        locale_emscripten
        environment_emscripten
        resource_emscripten
        virtualization_emscripten
        hardware_emscripten
        power_emscripten
        sensor_emscripten
        security_emscripten
        numa_emscripten
        process_list_emscripten
        printer_emscripten
        audio_emscripten
        software_emscripten
        storage_emscripten
        ipc_emscripten
        connection_emscripten
        gpu_emscripten
        display_emscripten
        input_emscripten
        camera_emscripten
        bluetooth_emscripten
        wifi_emscripten)
    foreach(emscripten_target IN LISTS EMSCRIPTEN_TEST_TARGETS)
        syscape_add_test(${emscripten_target} 17 syscape::hosted ${emscripten_target}.cpp)
        target_compile_definitions(${emscripten_target} PRIVATE EMSCRIPTEN=1 __EMSCRIPTEN__=1)
    endforeach()

    foreach(emscripten_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(emscripten_header_target "header_${emscripten_domain}_emscripten")
        syscape_add_test(
            ${emscripten_header_target}
            17
            syscape::hosted
            header_${emscripten_domain}.cpp)
        target_compile_definitions(${emscripten_header_target} PRIVATE EMSCRIPTEN=1 __EMSCRIPTEN__=1)
    endforeach()

    syscape_add_test(
        emscripten_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(emscripten_odr PRIVATE EMSCRIPTEN=1 __EMSCRIPTEN__=1)

    syscape_add_test(emscripten_detection 11 syscape::syscape emscripten_detection.cpp)
    target_compile_definitions(emscripten_detection PRIVATE EMSCRIPTEN=1 __EMSCRIPTEN__=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Fuchsia" OR FUCHSIA)
    set(FUCHSIA_TEST_TARGETS
        os_fuchsia
        cpu_fuchsia
        memory_fuchsia
        process_fuchsia
        user_fuchsia
        filesystem_fuchsia
        network_fuchsia
        locale_fuchsia
        environment_fuchsia
        resource_fuchsia
        virtualization_fuchsia
        hardware_fuchsia
        power_fuchsia
        sensor_fuchsia
        security_fuchsia
        numa_fuchsia
        process_list_fuchsia
        printer_fuchsia
        audio_fuchsia
        software_fuchsia
        storage_fuchsia
        ipc_fuchsia
        connection_fuchsia
        gpu_fuchsia
        display_fuchsia
        input_fuchsia
        camera_fuchsia
        bluetooth_fuchsia
        wifi_fuchsia)
    foreach(fuchsia_target IN LISTS FUCHSIA_TEST_TARGETS)
        syscape_add_test(${fuchsia_target} 17 syscape::hosted ${fuchsia_target}.cpp)
        target_compile_definitions(${fuchsia_target} PRIVATE FUCHSIA=1 __Fuchsia__=1)
    endforeach()

    foreach(fuchsia_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(fuchsia_header_target "header_${fuchsia_domain}_fuchsia")
        syscape_add_test(
            ${fuchsia_header_target}
            17
            syscape::hosted
            header_${fuchsia_domain}.cpp)
        target_compile_definitions(${fuchsia_header_target} PRIVATE FUCHSIA=1 __Fuchsia__=1)
    endforeach()

    syscape_add_test(
        fuchsia_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(fuchsia_odr PRIVATE FUCHSIA=1 __Fuchsia__=1)

    syscape_add_test(fuchsia_detection 11 syscape::syscape fuchsia_detection.cpp)
    target_compile_definitions(fuchsia_detection PRIVATE FUCHSIA=1 __Fuchsia__=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "FreeRTOS" OR FREERTOS)
    add_library(freertos_compat STATIC compat/freertos/freertos_compat.cpp)
    target_include_directories(freertos_compat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/compat/freertos)
    target_compile_features(freertos_compat PUBLIC cxx_std_17)

    set(FREERTOS_TEST_TARGETS
        os_freertos
        cpu_freertos
        memory_freertos
        process_freertos
        user_freertos
        filesystem_freertos
        network_freertos
        locale_freertos
        environment_freertos
        resource_freertos
        virtualization_freertos
        hardware_freertos
        power_freertos
        sensor_freertos
        security_freertos
        numa_freertos
        process_list_freertos
        printer_freertos
        audio_freertos
        software_freertos
        storage_freertos
        ipc_freertos
        connection_freertos
        gpu_freertos
        display_freertos
        input_freertos
        camera_freertos
        bluetooth_freertos
        wifi_freertos)
    foreach(freertos_target IN LISTS FREERTOS_TEST_TARGETS)
        syscape_add_test(${freertos_target} 17 syscape::hosted ${freertos_target}.cpp)
        target_compile_definitions(${freertos_target} PRIVATE FREERTOS=1 __FREERTOS__=1)
        target_link_libraries(${freertos_target} PRIVATE freertos_compat)
    endforeach()

    foreach(freertos_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(freertos_header_target "header_${freertos_domain}_freertos")
        syscape_add_test(
            ${freertos_header_target}
            17
            syscape::hosted
            header_${freertos_domain}.cpp)
        target_compile_definitions(${freertos_header_target} PRIVATE FREERTOS=1 __FREERTOS__=1)
        target_link_libraries(${freertos_header_target} PRIVATE freertos_compat)
    endforeach()

    syscape_add_test(
        freertos_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(freertos_odr PRIVATE FREERTOS=1 __FREERTOS__=1)
    target_link_libraries(freertos_odr PRIVATE freertos_compat)

    syscape_add_test(freertos_detection 11 syscape::syscape freertos_detection.cpp)
    target_compile_definitions(freertos_detection PRIVATE FREERTOS=1 __FREERTOS__=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "ThreadX" OR THREADX)
    add_library(threadx_compat STATIC compat/threadx/threadx_compat.cpp)
    target_include_directories(threadx_compat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/compat/threadx)
    target_compile_features(threadx_compat PUBLIC cxx_std_17)

    set(THREADX_TEST_TARGETS
        os_threadx
        cpu_threadx
        memory_threadx
        process_threadx
        user_threadx
        filesystem_threadx
        network_threadx
        locale_threadx
        environment_threadx
        resource_threadx
        virtualization_threadx
        hardware_threadx
        power_threadx
        sensor_threadx
        security_threadx
        numa_threadx
        process_list_threadx
        printer_threadx
        audio_threadx
        software_threadx
        storage_threadx
        ipc_threadx
        connection_threadx
        gpu_threadx
        display_threadx
        input_threadx
        camera_threadx
        bluetooth_threadx
        wifi_threadx)
    foreach(threadx_target IN LISTS THREADX_TEST_TARGETS)
        syscape_add_test(${threadx_target} 17 syscape::hosted ${threadx_target}.cpp)
        target_compile_definitions(${threadx_target} PRIVATE THREADX=1 __THREADX__=1)
        target_link_libraries(${threadx_target} PRIVATE threadx_compat)
    endforeach()

    foreach(threadx_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(threadx_header_target "header_${threadx_domain}_threadx")
        syscape_add_test(
            ${threadx_header_target}
            17
            syscape::hosted
            header_${threadx_domain}.cpp)
        target_compile_definitions(${threadx_header_target} PRIVATE THREADX=1 __THREADX__=1)
        target_link_libraries(${threadx_header_target} PRIVATE threadx_compat)
    endforeach()

    syscape_add_test(
        threadx_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(threadx_odr PRIVATE THREADX=1 __THREADX__=1)
    target_link_libraries(threadx_odr PRIVATE threadx_compat)

    syscape_add_test(threadx_detection 11 syscape::syscape threadx_detection.cpp)
    target_compile_definitions(threadx_detection PRIVATE THREADX=1 __THREADX__=1)
endif()

if(APPLE)
    target_link_libraries(
        header_locale PRIVATE "-framework CoreFoundation")
    target_link_libraries(
        odr PRIVATE "-framework CoreFoundation")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "^(iOS|tvOS|watchOS|visionOS)$")
    syscape_add_test(os_apple_mobile 17 syscape::hosted os_apple_mobile.cpp)
    syscape_add_test(cpu_apple_mobile 17 syscape::hosted cpu_apple_mobile.cpp)
    syscape_add_test(memory_apple_mobile 17 syscape::hosted memory_apple_mobile.cpp)
    syscape_add_test(process_apple_mobile 17 syscape::hosted process_apple_mobile.cpp)
    syscape_add_test(user_apple_mobile 17 syscape::hosted user_apple_mobile.cpp)
    syscape_add_test(filesystem_apple_mobile 17 syscape::hosted filesystem_apple_mobile.cpp)
    syscape_add_test(network_apple_mobile 17 syscape::hosted network_apple_mobile.cpp)
    syscape_add_test(locale_apple_mobile 17 syscape::hosted locale_apple_mobile.cpp)
    syscape_add_test(environment_apple_mobile 17 syscape::hosted environment_apple_mobile.cpp)
    syscape_add_test(resource_apple_mobile 17 syscape::hosted resource_apple_mobile.cpp)
    syscape_add_test(virtualization_apple_mobile 17 syscape::hosted virtualization_apple_mobile.cpp)
    syscape_add_test(hardware_apple_mobile 17 syscape::hosted hardware_apple_mobile.cpp)
    syscape_add_test(power_apple_mobile 17 syscape::hosted power_apple_mobile.cpp)
    syscape_add_test(sensor_apple_mobile 17 syscape::hosted sensor_apple_mobile.cpp)
    syscape_add_test(security_apple_mobile 17 syscape::hosted security_apple_mobile.cpp)
    syscape_add_test(numa_apple_mobile 17 syscape::hosted numa_apple_mobile.cpp)
    syscape_add_test(process_list_apple_mobile 17 syscape::hosted process_list_apple_mobile.cpp)
    syscape_add_test(printer_apple_mobile 17 syscape::hosted printer_apple_mobile.cpp)
    syscape_add_test(audio_apple_mobile 17 syscape::hosted audio_apple_mobile.cpp)
    syscape_add_test(software_apple_mobile 17 syscape::hosted software_apple_mobile.cpp)
    syscape_add_test(storage_apple_mobile 17 syscape::hosted storage_apple_mobile.cpp)
    syscape_add_test(ipc_apple_mobile 17 syscape::hosted ipc_apple_mobile.cpp)
    syscape_add_test(connection_apple_mobile 17 syscape::hosted connection_apple_mobile.cpp)
    target_link_libraries(
        locale_apple_mobile PRIVATE "-framework CoreFoundation")
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
