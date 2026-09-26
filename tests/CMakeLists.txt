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
syscape_add_test(toolchain_version 11 syscape::syscape toolchain_version.cpp)

function(syscape_add_toolchain_mock_test name define_flag)
    syscape_add_test(${name} 11 syscape::syscape toolchain_mock_case.cpp)
    target_compile_definitions(${name} PRIVATE ${define_flag} ${ARGN})
endfunction()

syscape_add_toolchain_mock_test(toolchain_mock_intel_classic TEST_INTEL_CLASSIC __INTEL_COMPILER=1900 __INTEL_COMPILER_UPDATE=2)
syscape_add_toolchain_mock_test(toolchain_mock_intel_llvm TEST_INTEL_LLVM __INTEL_LLVM_COMPILER=20230201)
syscape_add_toolchain_mock_test(toolchain_mock_ibm_open_xl TEST_IBM_OPEN_XL __open_xl_version__=17 __open_xl_release__=1 __open_xl_modification__=2)
syscape_add_toolchain_mock_test(toolchain_mock_ibm_xl TEST_IBM_XL __xlC__=0x1301 __xlC_ver__=0x0200)
syscape_add_toolchain_mock_test(toolchain_mock_ibm_xl_legacy TEST_IBM_XL_LEGACY __IBMCPP__=1210)
syscape_add_toolchain_mock_test(toolchain_mock_sunpro TEST_SUNPRO __SUNPRO_CC=0x5150)
syscape_add_toolchain_mock_test(toolchain_mock_sunpro_legacy TEST_SUNPRO_LEGACY __SUNPRO_CC=0x590)
syscape_add_toolchain_mock_test(toolchain_mock_hp_acc TEST_HP_ACC __HP_aCC=62500)
syscape_add_toolchain_mock_test(toolchain_mock_iar TEST_IAR __IAR_SYSTEMS_ICC__=1 __VER__=8050001)
syscape_add_toolchain_mock_test(toolchain_mock_iar_legacy TEST_IAR_LEGACY __IAR_SYSTEMS_ICC__=1 __VER__=334)
syscape_add_toolchain_mock_test(toolchain_mock_arm_ac6 TEST_ARM_AC6 __ARMCOMPILER_VERSION=6160100)
syscape_add_toolchain_mock_test(toolchain_mock_arm_ac5 TEST_ARM_AC5 __ARMCC_VERSION=5060753)
syscape_add_toolchain_mock_test(toolchain_mock_arm_cc_only TEST_ARM_CC_ARM_ONLY __CC_ARM=1)
syscape_add_toolchain_mock_test(toolchain_mock_ghs TEST_GHS __ghs__=1 __GHS_VERSION_NUMBER__=202214)
syscape_add_toolchain_mock_test(toolchain_mock_ti TEST_TI __TI_COMPILER_VERSION__=20002005)
syscape_add_toolchain_mock_test(toolchain_mock_renesas TEST_RENESAS __RENESAS_VERSION__=0x03010200)
syscape_add_toolchain_mock_test(toolchain_mock_renesas_legacy TEST_RENESAS_LEGACY __RENESAS_VERSION__=0x0203)
syscape_add_toolchain_mock_test(toolchain_mock_microchip_xc32 TEST_MICROCHIP_XC32 __XC32_VERSION=2321)
syscape_add_toolchain_mock_test(toolchain_mock_microchip_xc8 TEST_MICROCHIP_XC8 __XC8_VERSION=231)
syscape_add_toolchain_mock_test(toolchain_mock_openwatcom TEST_OPENWATCOM __WATCOMC__=1280)
syscape_add_toolchain_mock_test(toolchain_mock_openwatcom_v2 TEST_OPENWATCOM_V2 __WATCOMC__=1300)
syscape_add_toolchain_mock_test(toolchain_mock_watcom_legacy TEST_WATCOM_LEGACY __WATCOMC__=1100)

syscape_add_test(header_board 11 syscape::syscape header_board.cpp)
syscape_add_test(board_provider_test 11 syscape::syscape board_provider_test.cpp)
syscape_add_test(board_family_strings 11 syscape::syscape board_family_strings.cpp)
syscape_add_test(static_board_provider_test 11 syscape::syscape static_board_provider_test.cpp)

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

if(CMAKE_SYSTEM_NAME STREQUAL "embOS" OR EMBOS)
    add_library(embos_compat STATIC compat/embos/embos_compat.cpp)
    target_include_directories(embos_compat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/compat/embos)
    target_compile_features(embos_compat PUBLIC cxx_std_17)

    set(EMBOS_TEST_TARGETS
        os_embos
        cpu_embos
        memory_embos
        process_embos
        user_embos
        filesystem_embos
        network_embos
        locale_embos
        environment_embos
        resource_embos
        virtualization_embos
        hardware_embos
        power_embos
        sensor_embos
        security_embos
        numa_embos
        process_list_embos
        printer_embos
        audio_embos
        software_embos
        storage_embos
        ipc_embos
        connection_embos
        gpu_embos
        display_embos
        input_embos
        camera_embos
        bluetooth_embos
        wifi_embos)
    foreach(embos_target IN LISTS EMBOS_TEST_TARGETS)
        syscape_add_test(${embos_target} 17 syscape::hosted ${embos_target}.cpp)
        target_compile_definitions(${embos_target} PRIVATE EMBOS=1 __EMBOS__=1)
        target_link_libraries(${embos_target} PRIVATE embos_compat)
    endforeach()

    foreach(embos_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(embos_header_target "header_${embos_domain}_embos")
        syscape_add_test(
            ${embos_header_target}
            17
            syscape::hosted
            header_${embos_domain}.cpp)
        target_compile_definitions(${embos_header_target} PRIVATE EMBOS=1 __EMBOS__=1)
        target_link_libraries(${embos_header_target} PRIVATE embos_compat)
    endforeach()

    syscape_add_test(
        embos_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(embos_odr PRIVATE EMBOS=1 __EMBOS__=1)
    target_link_libraries(embos_odr PRIVATE embos_compat)

    syscape_add_test(embos_detection 11 syscape::syscape embos_detection.cpp)
    target_compile_definitions(embos_detection PRIVATE EMBOS=1 __EMBOS__=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "uCOS" OR CMAKE_SYSTEM_NAME STREQUAL "uC/OS" OR UCOS)
    add_library(ucos_compat STATIC compat/ucos/ucos_compat.cpp)
    target_include_directories(ucos_compat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/compat/ucos)
    target_compile_features(ucos_compat PUBLIC cxx_std_17)

    set(UCOS_TEST_TARGETS
        os_ucos
        cpu_ucos
        memory_ucos
        process_ucos
        user_ucos
        filesystem_ucos
        network_ucos
        locale_ucos
        environment_ucos
        resource_ucos
        virtualization_ucos
        hardware_ucos
        power_ucos
        sensor_ucos
        security_ucos
        numa_ucos
        process_list_ucos
        printer_ucos
        audio_ucos
        software_ucos
        storage_ucos
        ipc_ucos
        connection_ucos
        gpu_ucos
        display_ucos
        input_ucos
        camera_ucos
        bluetooth_ucos
        wifi_ucos)
    foreach(ucos_target IN LISTS UCOS_TEST_TARGETS)
        syscape_add_test(${ucos_target} 17 syscape::hosted ${ucos_target}.cpp)
        target_compile_definitions(${ucos_target} PRIVATE UCOS=1 __UCOS__=1 UCOS_III=1)
        target_link_libraries(${ucos_target} PRIVATE ucos_compat)
    endforeach()

    foreach(ucos_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(ucos_header_target "header_${ucos_domain}_ucos")
        syscape_add_test(
            ${ucos_header_target}
            17
            syscape::hosted
            header_${ucos_domain}.cpp)
        target_compile_definitions(${ucos_header_target} PRIVATE UCOS=1 __UCOS__=1 UCOS_III=1)
        target_link_libraries(${ucos_header_target} PRIVATE ucos_compat)
    endforeach()

    syscape_add_test(
        ucos_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(ucos_odr PRIVATE UCOS=1 __UCOS__=1 UCOS_III=1)
    target_link_libraries(ucos_odr PRIVATE ucos_compat)

    syscape_add_test(ucos_detection 11 syscape::syscape ucos_detection.cpp)
    target_compile_definitions(ucos_detection PRIVATE UCOS=1 __UCOS__=1 UCOS_III=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "INTEGRITY" OR INTEGRITY)
    add_library(integrity_compat STATIC compat/integrity/integrity_compat.cpp)
    target_include_directories(integrity_compat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/compat/integrity)
    target_compile_features(integrity_compat PUBLIC cxx_std_17)

    set(INTEGRITY_TEST_TARGETS
        os_integrity
        cpu_integrity
        memory_integrity
        process_integrity
        user_integrity
        filesystem_integrity
        network_integrity
        locale_integrity
        environment_integrity
        resource_integrity
        virtualization_integrity
        hardware_integrity
        power_integrity
        sensor_integrity
        security_integrity
        numa_integrity
        process_list_integrity
        printer_integrity
        audio_integrity
        software_integrity
        storage_integrity
        ipc_integrity
        connection_integrity
        gpu_integrity
        display_integrity
        input_integrity
        camera_integrity
        bluetooth_integrity
        wifi_integrity)
    foreach(integrity_target IN LISTS INTEGRITY_TEST_TARGETS)
        syscape_add_test(${integrity_target} 17 syscape::hosted ${integrity_target}.cpp)
        target_compile_definitions(${integrity_target} PRIVATE __INTEGRITY=1 INTEGRITY=1)
        target_link_libraries(${integrity_target} PRIVATE integrity_compat)
    endforeach()

    foreach(integrity_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(integrity_header_target "header_${integrity_domain}_integrity")
        syscape_add_test(
            ${integrity_header_target}
            17
            syscape::hosted
            header_${integrity_domain}.cpp)
        target_compile_definitions(${integrity_header_target} PRIVATE __INTEGRITY=1 INTEGRITY=1)
        target_link_libraries(${integrity_header_target} PRIVATE integrity_compat)
    endforeach()

    syscape_add_test(
        integrity_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(integrity_odr PRIVATE __INTEGRITY=1 INTEGRITY=1)
    target_link_libraries(integrity_odr PRIVATE integrity_compat)

    syscape_add_test(integrity_detection 11 syscape::syscape integrity_detection.cpp)
    target_compile_definitions(integrity_detection PRIVATE __INTEGRITY=1 INTEGRITY=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "TKERNEL" OR TKERNEL)
    add_library(tkernel_compat STATIC compat/tkernel/tkernel_compat.cpp)
    target_include_directories(tkernel_compat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/compat/tkernel)
    target_compile_features(tkernel_compat PUBLIC cxx_std_17)

    set(TKERNEL_TEST_TARGETS
        os_tkernel
        cpu_tkernel
        memory_tkernel
        process_tkernel
        user_tkernel
        filesystem_tkernel
        network_tkernel
        locale_tkernel
        environment_tkernel
        resource_tkernel
        virtualization_tkernel
        hardware_tkernel
        power_tkernel
        sensor_tkernel
        security_tkernel
        numa_tkernel
        process_list_tkernel
        printer_tkernel
        audio_tkernel
        software_tkernel
        storage_tkernel
        ipc_tkernel
        connection_tkernel
        gpu_tkernel
        display_tkernel
        input_tkernel
        camera_tkernel
        bluetooth_tkernel
        wifi_tkernel)
    foreach(tkernel_target IN LISTS TKERNEL_TEST_TARGETS)
        syscape_add_test(${tkernel_target} 17 syscape::hosted ${tkernel_target}.cpp)
        target_compile_definitions(${tkernel_target} PRIVATE _TKERNEL_=1 __TKERNEL__=1)
        target_link_libraries(${tkernel_target} PRIVATE tkernel_compat)
    endforeach()

    foreach(tkernel_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(tkernel_header_target "header_${tkernel_domain}_tkernel")
        syscape_add_test(
            ${tkernel_header_target}
            17
            syscape::hosted
            header_${tkernel_domain}.cpp)
        target_compile_definitions(${tkernel_header_target} PRIVATE _TKERNEL_=1 __TKERNEL__=1)
        target_link_libraries(${tkernel_header_target} PRIVATE tkernel_compat)
    endforeach()

    syscape_add_test(
        tkernel_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(tkernel_odr PRIVATE _TKERNEL_=1 __TKERNEL__=1)
    target_link_libraries(tkernel_odr PRIVATE tkernel_compat)

    syscape_add_test(tkernel_detection 11 syscape::syscape tkernel_detection.cpp)
    target_compile_definitions(tkernel_detection PRIVATE _TKERNEL_=1 __TKERNEL__=1)

    syscape_add_test(os_tkernel_ref_ver 17 syscape::hosted os_tkernel.cpp)
    target_compile_definitions(os_tkernel_ref_ver PRIVATE _TKERNEL_=1 __TKERNEL__=1 TK_NO_VERSION_MACRO=1)
    target_link_libraries(os_tkernel_ref_ver PRIVATE tkernel_compat)

    syscape_add_test(os_tkernel_macro_version 17 syscape::hosted os_tkernel.cpp)
    target_compile_definitions(os_tkernel_macro_version PRIVATE _TKERNEL_=1 __TKERNEL__=1 TK_MAJOR_VERSION=3 TK_MINOR_VERSION=0 TK_PATCH_VERSION=1)
    target_link_libraries(os_tkernel_macro_version PRIVATE tkernel_compat)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "CHIBIOS" OR CHIBIOS)
    add_library(chibios_compat STATIC compat/chibios/chibios_compat.cpp)
    target_include_directories(chibios_compat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/compat/chibios)
    target_compile_features(chibios_compat PUBLIC cxx_std_17)

    set(CHIBIOS_TEST_TARGETS
        os_chibios
        cpu_chibios
        memory_chibios
        process_chibios
        user_chibios
        filesystem_chibios
        network_chibios
        locale_chibios
        environment_chibios
        resource_chibios
        virtualization_chibios
        hardware_chibios
        power_chibios
        sensor_chibios
        security_chibios
        numa_chibios
        process_list_chibios
        printer_chibios
        audio_chibios
        software_chibios
        storage_chibios
        ipc_chibios
        connection_chibios
        gpu_chibios
        display_chibios
        input_chibios
        camera_chibios
        bluetooth_chibios
        wifi_chibios)
    foreach(chibios_target IN LISTS CHIBIOS_TEST_TARGETS)
        syscape_add_test(${chibios_target} 17 syscape::hosted ${chibios_target}.cpp)
        target_compile_definitions(${chibios_target} PRIVATE __CHIBIOS__=1 CHIBIOS=1)
        target_link_libraries(${chibios_target} PRIVATE chibios_compat)
    endforeach()

    foreach(chibios_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(chibios_header_target "header_${chibios_domain}_chibios")
        syscape_add_test(
            ${chibios_header_target}
            17
            syscape::hosted
            header_${chibios_domain}.cpp)
        target_compile_definitions(${chibios_header_target} PRIVATE __CHIBIOS__=1 CHIBIOS=1)
        target_link_libraries(${chibios_header_target} PRIVATE chibios_compat)
    endforeach()

    syscape_add_test(
        chibios_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(chibios_odr PRIVATE __CHIBIOS__=1 CHIBIOS=1)
    target_link_libraries(chibios_odr PRIVATE chibios_compat)

    add_library(chibios_compat_nil STATIC compat/chibios/chibios_compat.cpp)
    target_include_directories(chibios_compat_nil PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/compat/chibios)
    target_compile_features(chibios_compat_nil PUBLIC cxx_std_17)
    target_compile_definitions(chibios_compat_nil PRIVATE __CHIBIOS__=1 CHIBIOS=1 __CHIBIOS_NIL__=1 CH_USE_MACRO_APIS=1)

    syscape_add_test(chibios_detection 11 syscape::syscape chibios_detection.cpp)
    target_compile_definitions(chibios_detection PRIVATE __CHIBIOS__=1 CHIBIOS=1)

    syscape_add_test(os_chibios_nil 17 syscape::hosted os_chibios.cpp)
    target_compile_definitions(os_chibios_nil PRIVATE __CHIBIOS__=1 CHIBIOS=1 __CHIBIOS_NIL__=1 CH_USE_MACRO_APIS=1)
    target_link_libraries(os_chibios_nil PRIVATE chibios_compat_nil)

    syscape_add_test(process_chibios_nil 17 syscape::hosted process_chibios.cpp)
    target_compile_definitions(process_chibios_nil PRIVATE __CHIBIOS__=1 CHIBIOS=1 __CHIBIOS_NIL__=1 CH_USE_MACRO_APIS=1)
    target_link_libraries(process_chibios_nil PRIVATE chibios_compat_nil)

    syscape_add_test(resource_chibios_nil 17 syscape::hosted resource_chibios.cpp)
    target_compile_definitions(resource_chibios_nil PRIVATE __CHIBIOS__=1 CHIBIOS=1 __CHIBIOS_NIL__=1 CH_CFG_USE_REGISTRY=0 CH_CFG_MAX_THREADS=4)
    target_link_libraries(resource_chibios_nil PRIVATE chibios_compat)

    syscape_add_test(os_chibios_macro_version 17 syscape::hosted os_chibios.cpp)
    target_compile_definitions(os_chibios_macro_version PRIVATE __CHIBIOS__=1 CHIBIOS=1 CH_NO_VERSION_MACRO=1 CH_KERNEL_MAJOR=8 CH_KERNEL_MINOR=0 CH_KERNEL_PATCH=1)
    target_link_libraries(os_chibios_macro_version PRIVATE chibios_compat)

    syscape_add_test(os_chibios_no_freq 17 syscape::hosted os_chibios.cpp)
    target_compile_definitions(os_chibios_no_freq PRIVATE __CHIBIOS__=1 CHIBIOS=1 CH_NO_ST_FREQUENCY=1)
    target_link_libraries(os_chibios_no_freq PRIVATE chibios_compat)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "MYNEWT" OR MYNEWT)
    add_library(mynewt_compat STATIC compat/mynewt/mynewt_compat.cpp)
    target_include_directories(mynewt_compat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/compat/mynewt)
    target_compile_features(mynewt_compat PUBLIC cxx_std_17)

    set(MYNEWT_TEST_TARGETS
        os_mynewt
        cpu_mynewt
        memory_mynewt
        process_mynewt
        user_mynewt
        filesystem_mynewt
        network_mynewt
        locale_mynewt
        environment_mynewt
        resource_mynewt
        virtualization_mynewt
        hardware_mynewt
        power_mynewt
        sensor_mynewt
        security_mynewt
        numa_mynewt
        process_list_mynewt
        printer_mynewt
        audio_mynewt
        software_mynewt
        storage_mynewt
        ipc_mynewt
        connection_mynewt
        gpu_mynewt
        display_mynewt
        input_mynewt
        camera_mynewt
        bluetooth_mynewt
        wifi_mynewt)
    foreach(mynewt_target IN LISTS MYNEWT_TEST_TARGETS)
        syscape_add_test(${mynewt_target} 17 syscape::hosted ${mynewt_target}.cpp)
        target_compile_definitions(${mynewt_target} PRIVATE __MYNEWT__=1 MYNEWT=1)
        target_link_libraries(${mynewt_target} PRIVATE mynewt_compat)
    endforeach()

    foreach(mynewt_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(mynewt_header_target "header_${mynewt_domain}_mynewt")
        syscape_add_test(
            ${mynewt_header_target}
            17
            syscape::hosted
            header_${mynewt_domain}.cpp)
        target_compile_definitions(${mynewt_header_target} PRIVATE __MYNEWT__=1 MYNEWT=1)
        target_link_libraries(${mynewt_header_target} PRIVATE mynewt_compat)
    endforeach()

    syscape_add_test(
        mynewt_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(mynewt_odr PRIVATE __MYNEWT__=1 MYNEWT=1)
    target_link_libraries(mynewt_odr PRIVATE mynewt_compat)

    syscape_add_test(mynewt_detection 11 syscape::syscape mynewt_detection.cpp)
    target_compile_definitions(mynewt_detection PRIVATE __MYNEWT__=1 MYNEWT=1)

    syscape_add_test(os_mynewt_macro_version 17 syscape::hosted os_mynewt.cpp)
    target_compile_definitions(os_mynewt_macro_version PRIVATE __MYNEWT__=1 MYNEWT=1 MYNEWT_NO_VERSION_MACRO=1 MYNEWT_VERSION_MAJOR=1 MYNEWT_VERSION_MINOR=10 MYNEWT_VERSION_REVISION=0)
    target_link_libraries(os_mynewt_macro_version PRIVATE mynewt_compat)

    syscape_add_test(os_mynewt_no_freq 17 syscape::hosted os_mynewt.cpp)
    target_compile_definitions(os_mynewt_no_freq PRIVATE __MYNEWT__=1 MYNEWT=1 MYNEWT_NO_TICKS_PER_SEC=1)
    target_link_libraries(os_mynewt_no_freq PRIVATE mynewt_compat)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "MBED" OR MBED)
    add_library(mbed_compat STATIC compat/mbed/mbed_compat.cpp)
    target_include_directories(mbed_compat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/compat/mbed)
    target_compile_features(mbed_compat PUBLIC cxx_std_17)

    set(MBED_TEST_TARGETS
        os_mbed
        cpu_mbed
        memory_mbed
        process_mbed
        user_mbed
        filesystem_mbed
        network_mbed
        locale_mbed
        environment_mbed
        resource_mbed
        virtualization_mbed
        hardware_mbed
        power_mbed
        sensor_mbed
        security_mbed
        numa_mbed
        process_list_mbed
        printer_mbed
        audio_mbed
        software_mbed
        storage_mbed
        ipc_mbed
        connection_mbed
        gpu_mbed
        display_mbed
        input_mbed
        camera_mbed
        bluetooth_mbed
        wifi_mbed)
    foreach(mbed_target IN LISTS MBED_TEST_TARGETS)
        syscape_add_test(${mbed_target} 17 syscape::hosted ${mbed_target}.cpp)
        target_compile_definitions(${mbed_target} PRIVATE __MBED__=1 MBED=1)
        target_link_libraries(${mbed_target} PRIVATE mbed_compat)
    endforeach()

    foreach(mbed_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(mbed_header_target "header_${mbed_domain}_mbed")
        syscape_add_test(
            ${mbed_header_target}
            17
            syscape::hosted
            header_${mbed_domain}.cpp)
        target_compile_definitions(${mbed_header_target} PRIVATE __MBED__=1 MBED=1)
        target_link_libraries(${mbed_header_target} PRIVATE mbed_compat)
    endforeach()

    syscape_add_test(
        mbed_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(mbed_odr PRIVATE __MBED__=1 MBED=1)
    target_link_libraries(mbed_odr PRIVATE mbed_compat)

    syscape_add_test(mbed_detection 11 syscape::syscape mbed_detection.cpp)
    target_compile_definitions(mbed_detection PRIVATE __MBED__=1 MBED=1)

    syscape_add_test(os_mbed_macro_version 17 syscape::hosted os_mbed.cpp)
    target_compile_definitions(os_mbed_macro_version PRIVATE __MBED__=1 MBED=1 MBED_NO_VERSION_MACRO=1 MBED_MAJOR_VERSION=6 MBED_MINOR_VERSION=15 MBED_PATCH_VERSION=0)
    target_link_libraries(os_mbed_macro_version PRIVATE mbed_compat)

    syscape_add_test(os_mbed_no_freq 17 syscape::hosted os_mbed.cpp)
    target_compile_definitions(os_mbed_no_freq PRIVATE __MBED__=1 MBED=1 MBED_NO_TICK_FREQ=1)
    target_link_libraries(os_mbed_no_freq PRIVATE mbed_compat)

    syscape_add_test(os_mbed_macro_freq 17 syscape::hosted os_mbed.cpp)
    target_compile_definitions(os_mbed_macro_freq PRIVATE __MBED__=1 MBED=1 MBED_USE_TICK_FREQ_MACRO=1)
    target_link_libraries(os_mbed_macro_freq PRIVATE mbed_compat)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "RIOT" OR RIOT)
    add_library(riot_compat STATIC compat/riot/riot_compat.cpp)
    target_include_directories(riot_compat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/compat/riot)
    target_compile_features(riot_compat PUBLIC cxx_std_17)

    set(RIOT_TEST_TARGETS
        os_riot
        cpu_riot
        memory_riot
        process_riot
        user_riot
        filesystem_riot
        network_riot
        locale_riot
        environment_riot
        resource_riot
        virtualization_riot
        hardware_riot
        power_riot
        sensor_riot
        security_riot
        numa_riot
        process_list_riot
        printer_riot
        audio_riot
        software_riot
        storage_riot
        ipc_riot
        connection_riot
        gpu_riot
        display_riot
        input_riot
        camera_riot
        bluetooth_riot
        wifi_riot)
    foreach(riot_target IN LISTS RIOT_TEST_TARGETS)
        syscape_add_test(${riot_target} 17 syscape::hosted ${riot_target}.cpp)
        target_compile_definitions(${riot_target} PRIVATE __RIOT__=1 RIOT=1)
        target_link_libraries(${riot_target} PRIVATE riot_compat)
    endforeach()

    foreach(riot_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(riot_header_target "header_${riot_domain}_riot")
        syscape_add_test(
            ${riot_header_target}
            17
            syscape::hosted
            header_${riot_domain}.cpp)
        target_compile_definitions(${riot_header_target} PRIVATE __RIOT__=1 RIOT=1)
        target_link_libraries(${riot_header_target} PRIVATE riot_compat)
    endforeach()

    syscape_add_test(
        riot_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(riot_odr PRIVATE __RIOT__=1 RIOT=1)
    target_link_libraries(riot_odr PRIVATE riot_compat)

    syscape_add_test(riot_detection 11 syscape::syscape riot_detection.cpp)
    target_compile_definitions(riot_detection PRIVATE __RIOT__=1 RIOT=1)

    syscape_add_test(os_riot_no_timer 17 syscape::hosted os_riot.cpp)
    target_compile_definitions(os_riot_no_timer PRIVATE __RIOT__=1 RIOT=1 RIOT_NO_TIMER=1)
    target_link_libraries(os_riot_no_timer PRIVATE riot_compat)

    syscape_add_test(os_riot_xtimer 17 syscape::hosted os_riot.cpp)
    target_compile_definitions(os_riot_xtimer PRIVATE __RIOT__=1 RIOT=1 RIOT_USE_XTIMER=1)
    target_link_libraries(os_riot_xtimer PRIVATE riot_compat)

    syscape_add_test(os_riot_ztimer32 17 syscape::hosted os_riot.cpp)
    target_compile_definitions(os_riot_ztimer32 PRIVATE __RIOT__=1 RIOT=1 RIOT_USE_ZTIMER32=1)
    target_link_libraries(os_riot_ztimer32 PRIVATE riot_compat)

    syscape_add_test(os_riot_ztimer_only 17 syscape::hosted os_riot.cpp)
    target_compile_definitions(os_riot_ztimer_only PRIVATE __RIOT__=1 RIOT=1 RIOT_NO_ZTIMER64=1)
    target_link_libraries(os_riot_ztimer_only PRIVATE riot_compat)
endif()

# Compatibility environments and variants (Tier 3)
syscape_add_test(cygwin_detection 11 syscape::syscape cygwin_detection.cpp)
target_compile_definitions(cygwin_detection PRIVATE __CYGWIN__=1)

syscape_add_test(mingw_detection 11 syscape::syscape mingw_detection.cpp)
target_compile_definitions(mingw_detection PRIVATE __MINGW32__=1 _WIN32=1)

syscape_add_test(msys2_detection 11 syscape::syscape msys2_detection.cpp)
target_compile_definitions(msys2_detection PRIVATE __MSYS__=1 _WIN32=1)

syscape_add_test(compat_env_strings 11 syscape::syscape compat_env_strings.cpp)

syscape_add_test(wine_virtualization 17 syscape::hosted wine_virtualization.cpp)

if(CYGWIN OR CMAKE_SYSTEM_NAME STREQUAL "CYGWIN" OR (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID))
    # Cygwin tests can run on POSIX host using SYSCAPE_TARGET_CYGWIN=1
    syscape_add_test(cygwin_functional 17 syscape::hosted cygwin_functional.cpp)
    target_compile_definitions(cygwin_functional PRIVATE SYSCAPE_TARGET_CYGWIN=1)

    syscape_add_test(
        cygwin_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(cygwin_odr PRIVATE SYSCAPE_TARGET_CYGWIN=1)

    foreach(cygwin_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(cygwin_header_target "header_${cygwin_domain}_cygwin")
        syscape_add_test(
            ${cygwin_header_target}
            17
            syscape::hosted
            header_${cygwin_domain}.cpp)
        target_compile_definitions(${cygwin_header_target} PRIVATE SYSCAPE_TARGET_CYGWIN=1)
    endforeach()
endif()

# Legacy operating systems and environments (Tier 4)
syscape_add_test(dos_detection 11 syscape::syscape dos_detection.cpp)
target_compile_definitions(dos_detection PRIVATE __MSDOS__=1 SYSCAPE_TARGET_DOS=1)

syscape_add_test(os2_detection 11 syscape::syscape os2_detection.cpp)
target_compile_definitions(os2_detection PRIVATE __OS2__=1 SYSCAPE_TARGET_OS2=1)

syscape_add_test(amigaos_detection 11 syscape::syscape amigaos_detection.cpp)
target_compile_definitions(amigaos_detection PRIVATE __AMIGA__=1 SYSCAPE_TARGET_AMIGAOS=1)

syscape_add_test(morphos_detection 11 syscape::syscape morphos_detection.cpp)
target_compile_definitions(morphos_detection PRIVATE __MORPHOS__=1 SYSCAPE_TARGET_AMIGAOS=1)

syscape_add_test(riscos_detection 11 syscape::syscape riscos_detection.cpp)
target_compile_definitions(riscos_detection PRIVATE __riscos__=1 SYSCAPE_TARGET_RISCOS=1)

syscape_add_test(openvms_detection 11 syscape::syscape openvms_detection.cpp)
target_compile_definitions(openvms_detection PRIVATE __VMS=1)

syscape_add_test(zos_detection 11 syscape::syscape zos_detection.cpp)
target_compile_definitions(zos_detection PRIVATE __MVS__=1)

syscape_add_test(ibmi_detection 11 syscape::syscape ibmi_detection.cpp)
target_compile_definitions(ibmi_detection PRIVATE __OS400__=1)

syscape_add_test(ibmi_pase_detection 11 syscape::syscape ibmi_detection.cpp)
target_compile_definitions(ibmi_pase_detection PRIVATE _AIX=1 _PASE=1)

syscape_add_test(tizen_detection 11 syscape::syscape tizen_detection.cpp)
target_compile_definitions(tizen_detection PRIVATE __TIZEN__=1)

syscape_add_test(sailfish_detection 11 syscape::syscape sailfish_detection.cpp)
target_compile_definitions(sailfish_detection PRIVATE __SAILFISH__=1)

syscape_add_test(kaios_detection 11 syscape::syscape kaios_detection.cpp)
target_compile_definitions(kaios_detection PRIVATE __KAIOS__=1)

syscape_add_test(legacy_env_strings 11 syscape::syscape legacy_env_strings.cpp)

# MCU and board families (Tier 4 / Freestanding Minimal)
syscape_add_test(mcu_avr_detection 11 syscape::syscape mcu_avr_detection.cpp)
target_compile_definitions(mcu_avr_detection PRIVATE __AVR__=1 SYSCAPE_TARGET_MCU_AVR=1)

syscape_add_test(mcu_sam_detection 11 syscape::syscape mcu_sam_detection.cpp)
target_compile_definitions(mcu_sam_detection PRIVATE __SAMD21__=1 SYSCAPE_TARGET_MCU_SAM=1)

syscape_add_test(mcu_esp_detection 11 syscape::syscape mcu_esp_detection.cpp)
target_compile_definitions(mcu_esp_detection PRIVATE ESP_PLATFORM=1 SYSCAPE_TARGET_MCU_ESP=1)

syscape_add_test(mcu_stm32_detection 11 syscape::syscape mcu_stm32_detection.cpp)
target_compile_definitions(mcu_stm32_detection PRIVATE STM32F4=1 SYSCAPE_TARGET_MCU_STM32=1)

syscape_add_test(mcu_rp_detection 11 syscape::syscape mcu_rp_detection.cpp)
target_compile_definitions(mcu_rp_detection PRIVATE PICO_RP2040=1 SYSCAPE_TARGET_MCU_RP=1)

syscape_add_test(mcu_nordic_detection 11 syscape::syscape mcu_nordic_detection.cpp)
target_compile_definitions(mcu_nordic_detection PRIVATE NRF52=1 SYSCAPE_TARGET_MCU_NORDIC=1)

syscape_add_test(mcu_ti_detection 11 syscape::syscape mcu_ti_detection.cpp)
target_compile_definitions(mcu_ti_detection PRIVATE __MSP430__=1 SYSCAPE_TARGET_MCU_TI=1)

syscape_add_test(mcu_nxp_detection 11 syscape::syscape mcu_nxp_detection.cpp)
target_compile_definitions(mcu_nxp_detection PRIVATE CPU_MIMXRT1062DVL6A=1 SYSCAPE_TARGET_MCU_NXP=1)

syscape_add_test(mcu_renesas_detection 11 syscape::syscape mcu_renesas_detection.cpp)
target_compile_definitions(mcu_renesas_detection PRIVATE __RX__=1 SYSCAPE_TARGET_MCU_RENESAS=1)

syscape_add_test(mcu_pic_detection 11 syscape::syscape mcu_pic_detection.cpp)
target_compile_definitions(mcu_pic_detection PRIVATE __PIC32MX__=1 SYSCAPE_TARGET_MCU_PIC=1)

syscape_add_test(mcu_gd32_detection 11 syscape::syscape mcu_gd32_detection.cpp)
target_compile_definitions(mcu_gd32_detection PRIVATE GD32F10X=1 SYSCAPE_TARGET_MCU_GD32=1)

syscape_add_test(mcu_ch32_detection 11 syscape::syscape mcu_ch32_detection.cpp)
target_compile_definitions(mcu_ch32_detection PRIVATE CH32V307=1 SYSCAPE_TARGET_MCU_CH32=1)

syscape_add_test(mcu_bouffalo_detection 11 syscape::syscape mcu_bouffalo_detection.cpp)
target_compile_definitions(mcu_bouffalo_detection PRIVATE BL602=1 SYSCAPE_TARGET_MCU_BOUFFALO=1)

syscape_add_test(mcu_sifive_detection 11 syscape::syscape mcu_sifive_detection.cpp)
target_compile_definitions(mcu_sifive_detection PRIVATE SIFIVE_FE310=1 SYSCAPE_TARGET_MCU_SIFIVE=1)

syscape_add_test(mcu_arduino_detection 11 syscape::syscape mcu_arduino_detection.cpp)
target_compile_definitions(mcu_arduino_detection PRIVATE ARDUINO=10819 SYSCAPE_TARGET_MCU_ARDUINO=1)

syscape_add_test(mcu_arduino_esp_priority 11 syscape::syscape mcu_arduino_esp_priority.cpp)
target_compile_definitions(mcu_arduino_esp_priority PRIVATE ARDUINO=10819 ESP_PLATFORM=1)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID)
    # DOS tests on POSIX host using SYSCAPE_TARGET_DOS=1
    syscape_add_test(dos_functional 17 syscape::hosted dos_functional.cpp)
    target_compile_definitions(dos_functional PRIVATE SYSCAPE_TARGET_DOS=1)

    syscape_add_test(
        dos_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(dos_odr PRIVATE SYSCAPE_TARGET_DOS=1)

    foreach(dos_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(dos_header_target "header_${dos_domain}_dos")
        syscape_add_test(
            ${dos_header_target}
            17
            syscape::hosted
            header_${dos_domain}.cpp)
        target_compile_definitions(${dos_header_target} PRIVATE SYSCAPE_TARGET_DOS=1)
    endforeach()

    # OS/2 tests on POSIX host using SYSCAPE_TARGET_OS2=1
    syscape_add_test(os2_functional 17 syscape::hosted os2_functional.cpp)
    target_compile_definitions(os2_functional PRIVATE SYSCAPE_TARGET_OS2=1)

    syscape_add_test(
        os2_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(os2_odr PRIVATE SYSCAPE_TARGET_OS2=1)

    foreach(os2_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(os2_header_target "header_${os2_domain}_os2")
        syscape_add_test(
            ${os2_header_target}
            17
            syscape::hosted
            header_${os2_domain}.cpp)
        target_compile_definitions(${os2_header_target} PRIVATE SYSCAPE_TARGET_OS2=1)
    endforeach()

    # AmigaOS tests on POSIX host using SYSCAPE_TARGET_AMIGAOS=1
    syscape_add_test(amigaos_functional 17 syscape::hosted amigaos_functional.cpp)
    target_compile_definitions(amigaos_functional PRIVATE SYSCAPE_TARGET_AMIGAOS=1)

    syscape_add_test(morphos_functional 17 syscape::hosted morphos_functional.cpp)
    target_compile_definitions(morphos_functional PRIVATE __MORPHOS__=1 SYSCAPE_TARGET_AMIGAOS=1)

    syscape_add_test(amigaos_overrides 17 syscape::hosted amigaos_overrides.cpp)
    target_compile_definitions(amigaos_overrides PRIVATE
        SYSCAPE_TARGET_AMIGAOS=1
        SYSCAPE_AMIGAOS_VERSION="3.9"
        SYSCAPE_AMIGAOS_KERNEL_VERSION="3.9"
        SYSCAPE_AMIGAOS_UPTIME_MS=3600000
        SYSCAPE_AMIGAOS_TOTAL_RAM_BYTES=16777216
        SYSCAPE_AMIGAOS_FREE_RAM_BYTES=4194304
        SYSCAPE_AMIGAOS_PID=123
        SYSCAPE_AMIGAOS_PPID=1
        SYSCAPE_AMIGAOS_THREAD_COUNT=4
        SYSCAPE_AMIGAOS_TASK_PRIORITY=10
        SYSCAPE_AMIGAOS_PROCESS_COUNT=8
        SYSCAPE_AMIGAOS_SYSTEM_THREAD_COUNT=16
        SYSCAPE_AMIGAOS_MAX_FILES=50)

    syscape_add_test(
        amigaos_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(amigaos_odr PRIVATE SYSCAPE_TARGET_AMIGAOS=1)

    foreach(amigaos_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(amigaos_header_target "header_${amigaos_domain}_amigaos")
        syscape_add_test(
            ${amigaos_header_target}
            17
            syscape::hosted
            header_${amigaos_domain}.cpp)
        target_compile_definitions(${amigaos_header_target} PRIVATE SYSCAPE_TARGET_AMIGAOS=1)
    endforeach()

    # RISC OS tests on POSIX host using SYSCAPE_TARGET_RISCOS=1
    syscape_add_test(riscos_functional 17 syscape::hosted riscos_functional.cpp)
    target_compile_definitions(riscos_functional PRIVATE SYSCAPE_TARGET_RISCOS=1)

    syscape_add_test(riscos_overrides 17 syscape::hosted riscos_overrides.cpp)
    target_compile_definitions(riscos_overrides PRIVATE
        SYSCAPE_TARGET_RISCOS=1
        SYSCAPE_RISCOS_VERSION="5.30"
        SYSCAPE_RISCOS_KERNEL_VERSION="5.30"
        SYSCAPE_RISCOS_UPTIME_MS=7200000
        SYSCAPE_RISCOS_TOTAL_RAM_BYTES=67108864
        SYSCAPE_RISCOS_FREE_RAM_BYTES=33554432
        SYSCAPE_RISCOS_PID=200
        SYSCAPE_RISCOS_PPID=1
        SYSCAPE_RISCOS_THREAD_COUNT=1
        SYSCAPE_RISCOS_TASK_PRIORITY=0
        SYSCAPE_RISCOS_CLASSIC_FS=1
        SYSCAPE_RISCOS_PROCESS_COUNT=5
        SYSCAPE_RISCOS_SYSTEM_THREAD_COUNT=5
        SYSCAPE_RISCOS_MAX_FILES=128)

    syscape_add_test(
        riscos_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(riscos_odr PRIVATE SYSCAPE_TARGET_RISCOS=1)

    foreach(riscos_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(riscos_header_target "header_${riscos_domain}_riscos")
        syscape_add_test(
            ${riscos_header_target}
            17
            syscape::hosted
            header_${riscos_domain}.cpp)
        target_compile_definitions(${riscos_header_target} PRIVATE SYSCAPE_TARGET_RISCOS=1)
    endforeach()

    # OpenVMS tests on POSIX host using SYSCAPE_TARGET_OPENVMS=1
    syscape_add_test(openvms_functional 17 syscape::hosted openvms_functional.cpp)
    target_compile_definitions(openvms_functional PRIVATE SYSCAPE_TARGET_OPENVMS=1)

    syscape_add_test(openvms_overrides 17 syscape::hosted openvms_overrides.cpp)
    target_compile_definitions(openvms_overrides PRIVATE
        SYSCAPE_TARGET_OPENVMS=1
        SYSCAPE_OPENVMS_VERSION="V8.4-2L1"
        SYSCAPE_OPENVMS_KERNEL_VERSION="V8.4-2L1"
        SYSCAPE_OPENVMS_HOST_NAME="NODE01"
        SYSCAPE_OPENVMS_UPTIME_MS=7200000
        SYSCAPE_OPENVMS_CPU_COUNT=4
        SYSCAPE_OPENVMS_PACKAGE_COUNT=1
        SYSCAPE_OPENVMS_PAGE_SIZE_BYTES=65536
        SYSCAPE_OPENVMS_TOTAL_RAM_BYTES=34359738368
        SYSCAPE_OPENVMS_FREE_RAM_BYTES=8589934592
        SYSCAPE_OPENVMS_PID=2048
        SYSCAPE_OPENVMS_PPID=1024
        SYSCAPE_OPENVMS_THREAD_COUNT=16
        SYSCAPE_OPENVMS_TASK_PRIORITY=16
        SYSCAPE_OPENVMS_ODS2=1
        SYSCAPE_OPENVMS_PROCESS_COUNT=64
        SYSCAPE_OPENVMS_SYSTEM_THREAD_COUNT=256
        SYSCAPE_OPENVMS_CHANNEL_LIMIT=4096)

    syscape_add_test(
        openvms_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(openvms_odr PRIVATE SYSCAPE_TARGET_OPENVMS=1)

    foreach(openvms_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(openvms_header_target "header_${openvms_domain}_openvms")
        syscape_add_test(
            ${openvms_header_target}
            17
            syscape::hosted
            header_${openvms_domain}.cpp)
        target_compile_definitions(${openvms_header_target} PRIVATE SYSCAPE_TARGET_OPENVMS=1)
    endforeach()

    # z/OS tests on POSIX host using SYSCAPE_TARGET_ZOS=1
    syscape_add_test(zos_functional 17 syscape::hosted zos_functional.cpp)
    target_compile_definitions(zos_functional PRIVATE SYSCAPE_TARGET_ZOS=1)

    syscape_add_test(zos_overrides 17 syscape::hosted zos_overrides.cpp)
    target_compile_definitions(zos_overrides PRIVATE
        SYSCAPE_TARGET_ZOS=1
        SYSCAPE_ZOS_VERSION="V2R5"
        SYSCAPE_ZOS_KERNEL_VERSION="V2R5"
        SYSCAPE_ZOS_HOST_NAME="MAINFRAME1"
        SYSCAPE_ZOS_UPTIME_MS=10800000
        SYSCAPE_ZOS_CPU_COUNT=8
        SYSCAPE_ZOS_PACKAGE_COUNT=1
        SYSCAPE_ZOS_PAGE_SIZE_BYTES=8192
        SYSCAPE_ZOS_TOTAL_RAM_BYTES=68719476736
        SYSCAPE_ZOS_FREE_RAM_BYTES=17179869184
        SYSCAPE_ZOS_PID=500
        SYSCAPE_ZOS_PPID=1
        SYSCAPE_ZOS_THREAD_COUNT=32
        SYSCAPE_ZOS_TASK_PRIORITY=0
        SYSCAPE_ZOS_PROCESS_COUNT=128
        SYSCAPE_ZOS_SYSTEM_THREAD_COUNT=1024
        SYSCAPE_ZOS_MAX_FILES=1024)

    syscape_add_test(
        zos_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(zos_odr PRIVATE SYSCAPE_TARGET_ZOS=1)

    foreach(zos_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(zos_header_target "header_${zos_domain}_zos")
        syscape_add_test(
            ${zos_header_target}
            17
            syscape::hosted
            header_${zos_domain}.cpp)
        target_compile_definitions(${zos_header_target} PRIVATE SYSCAPE_TARGET_ZOS=1)
    endforeach()

    # IBM i tests on POSIX host using SYSCAPE_TARGET_IBMI=1
    syscape_add_test(ibmi_functional 17 syscape::hosted ibmi_functional.cpp)
    target_compile_definitions(ibmi_functional PRIVATE SYSCAPE_TARGET_IBMI=1)

    syscape_add_test(ibmi_pase_functional 17 syscape::hosted ibmi_pase_functional.cpp)
    target_compile_definitions(ibmi_pase_functional PRIVATE SYSCAPE_TARGET_IBMI=1 _AIX=1 _PASE=1)

    syscape_add_test(ibmi_vrm_test 17 syscape::hosted ibmi_vrm_test.cpp)
    target_compile_definitions(ibmi_vrm_test PRIVATE SYSCAPE_TARGET_IBMI=1 __OS400_TGTVRM__=750)

    syscape_add_test(ibmi_overrides 17 syscape::hosted ibmi_overrides.cpp)
    target_compile_definitions(ibmi_overrides PRIVATE
        SYSCAPE_TARGET_IBMI=1
        SYSCAPE_IBMI_VERSION="V7R4M0"
        SYSCAPE_IBMI_KERNEL_VERSION="V7R4M0"
        SYSCAPE_IBMI_HOST_NAME="AS400"
        SYSCAPE_IBMI_UPTIME_MS=10800000
        SYSCAPE_IBMI_CPU_COUNT=8
        SYSCAPE_IBMI_PHYSICAL_CPU_COUNT=4
        SYSCAPE_IBMI_PACKAGE_COUNT=1
        SYSCAPE_IBMI_PAGE_SIZE_BYTES=8192
        SYSCAPE_IBMI_TOTAL_RAM_BYTES=68719476736
        SYSCAPE_IBMI_FREE_RAM_BYTES=17179869184
        SYSCAPE_IBMI_PID=500
        SYSCAPE_IBMI_PPID=1
        SYSCAPE_IBMI_THREAD_COUNT=32
        SYSCAPE_IBMI_TASK_PRIORITY=0
        SYSCAPE_IBMI_PROCESS_COUNT=128
        SYSCAPE_IBMI_SYSTEM_THREAD_COUNT=1024
        SYSCAPE_IBMI_MAX_FILES=1024
        SYSCAPE_IBMI_QSYS=1)

    syscape_add_test(
        ibmi_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(ibmi_odr PRIVATE SYSCAPE_TARGET_IBMI=1)

    foreach(ibmi_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(ibmi_header_target "header_${ibmi_domain}_ibmi")
        syscape_add_test(
            ${ibmi_header_target}
            17
            syscape::hosted
            header_${ibmi_domain}.cpp)
        target_compile_definitions(${ibmi_header_target} PRIVATE SYSCAPE_TARGET_IBMI=1)
    endforeach()

    # Tizen tests on POSIX host using SYSCAPE_TARGET_TIZEN=1
    syscape_add_test(tizen_functional 17 syscape::hosted tizen_functional.cpp)
    target_compile_definitions(tizen_functional PRIVATE SYSCAPE_TARGET_TIZEN=1)

    syscape_add_test(tizen_overrides 17 syscape::hosted tizen_overrides.cpp)
    target_compile_definitions(tizen_overrides PRIVATE
        SYSCAPE_TARGET_TIZEN=1
        SYSCAPE_TIZEN_VERSION="3.0.0"
        SYSCAPE_TIZEN_KERNEL_NAME="Linux-custom"
        SYSCAPE_TIZEN_KERNEL_VERSION="5.4.0"
        SYSCAPE_TIZEN_HOST_NAME="my-tizen-device"
        SYSCAPE_TIZEN_UPTIME_MS=123456
        SYSCAPE_TIZEN_CPU_COUNT=8
        SYSCAPE_TIZEN_PHYSICAL_CPU_COUNT=4
        SYSCAPE_TIZEN_PACKAGE_COUNT=1
        SYSCAPE_TIZEN_PAGE_SIZE_BYTES=4096
        SYSCAPE_TIZEN_TOTAL_RAM_BYTES=4294967296ULL
        SYSCAPE_TIZEN_FREE_RAM_BYTES=2147483648ULL
        SYSCAPE_TIZEN_PID=100
        SYSCAPE_TIZEN_PPID=1
        SYSCAPE_TIZEN_THREAD_COUNT=16
        SYSCAPE_TIZEN_PRIORITY=5
        SYSCAPE_TIZEN_NAME_MAX=128
        SYSCAPE_TIZEN_PATH_MAX=2048
        SYSCAPE_TIZEN_PROCESS_COUNT=42
        SYSCAPE_TIZEN_SYSTEM_THREAD_COUNT=128
        SYSCAPE_TIZEN_MAX_FILES=1024)

    syscape_add_test(tizen_invalid_overrides 17 syscape::hosted tizen_invalid_overrides.cpp)
    target_compile_definitions(tizen_invalid_overrides PRIVATE
        SYSCAPE_TARGET_TIZEN=1
        SYSCAPE_TIZEN_CPU_COUNT=0
        SYSCAPE_TIZEN_PHYSICAL_CPU_COUNT=5000000000ULL
        SYSCAPE_TIZEN_PACKAGE_COUNT=0
        SYSCAPE_TIZEN_TOTAL_RAM_BYTES=0ULL
        SYSCAPE_TIZEN_FREE_RAM_BYTES=-1
        SYSCAPE_TIZEN_PAGE_SIZE_BYTES=0
        SYSCAPE_TIZEN_PID=0
        SYSCAPE_TIZEN_PPID=-1
        SYSCAPE_TIZEN_THREAD_COUNT=5000000000ULL
        SYSCAPE_TIZEN_NAME_MAX=0
        SYSCAPE_TIZEN_PATH_MAX=0
        SYSCAPE_TIZEN_PROCESS_COUNT=0
        SYSCAPE_TIZEN_SYSTEM_THREAD_COUNT=0
        SYSCAPE_TIZEN_MAX_FILES=0
        SYSCAPE_TIZEN_UPTIME_MS=-1
        SYSCAPE_TIZEN_TASK_PRIORITY=5000000000ULL)

    syscape_add_test(
        tizen_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(tizen_odr PRIVATE SYSCAPE_TARGET_TIZEN=1)

    foreach(tizen_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(tizen_header_target "header_${tizen_domain}_tizen")
        syscape_add_test(
            ${tizen_header_target}
            17
            syscape::hosted
            header_${tizen_domain}.cpp)
        target_compile_definitions(${tizen_header_target} PRIVATE SYSCAPE_TARGET_TIZEN=1)
    endforeach()

    # Sailfish OS tests on POSIX host using SYSCAPE_TARGET_SAILFISH=1
    syscape_add_test(sailfish_functional 17 syscape::hosted sailfish_functional.cpp)
    target_compile_definitions(sailfish_functional PRIVATE SYSCAPE_TARGET_SAILFISH=1)

    syscape_add_test(sailfish_overrides 17 syscape::hosted sailfish_overrides.cpp)
    target_compile_definitions(sailfish_overrides PRIVATE
        SYSCAPE_TARGET_SAILFISH=1
        SYSCAPE_SAILFISH_VERSION="3.0.0"
        SYSCAPE_SAILFISH_KERNEL_NAME="Linux-custom"
        SYSCAPE_SAILFISH_KERNEL_VERSION="5.4.0"
        SYSCAPE_SAILFISH_HOST_NAME="my-sailfish-device"
        SYSCAPE_SAILFISH_UPTIME_MS=123456
        SYSCAPE_SAILFISH_CPU_COUNT=8
        SYSCAPE_SAILFISH_PHYSICAL_CPU_COUNT=4
        SYSCAPE_SAILFISH_PACKAGE_COUNT=1
        SYSCAPE_SAILFISH_PAGE_SIZE_BYTES=4096
        SYSCAPE_SAILFISH_TOTAL_RAM_BYTES=4294967296ULL
        SYSCAPE_SAILFISH_FREE_RAM_BYTES=2147483648ULL
        SYSCAPE_SAILFISH_PID=100
        SYSCAPE_SAILFISH_PPID=1
        SYSCAPE_SAILFISH_THREAD_COUNT=16
        SYSCAPE_SAILFISH_PRIORITY=5
        SYSCAPE_SAILFISH_NAME_MAX=128
        SYSCAPE_SAILFISH_PATH_MAX=2048
        SYSCAPE_SAILFISH_PROCESS_COUNT=42
        SYSCAPE_SAILFISH_SYSTEM_THREAD_COUNT=128
        SYSCAPE_SAILFISH_MAX_FILES=1024)

    syscape_add_test(sailfish_invalid_overrides 17 syscape::hosted sailfish_invalid_overrides.cpp)
    target_compile_definitions(sailfish_invalid_overrides PRIVATE
        SYSCAPE_TARGET_SAILFISH=1
        SYSCAPE_SAILFISH_CPU_COUNT=0
        SYSCAPE_SAILFISH_PHYSICAL_CPU_COUNT=5000000000ULL
        SYSCAPE_SAILFISH_PACKAGE_COUNT=0
        SYSCAPE_SAILFISH_TOTAL_RAM_BYTES=0ULL
        SYSCAPE_SAILFISH_FREE_RAM_BYTES=-1
        SYSCAPE_SAILFISH_PAGE_SIZE_BYTES=0
        SYSCAPE_SAILFISH_PID=0
        SYSCAPE_SAILFISH_PPID=-1
        SYSCAPE_SAILFISH_THREAD_COUNT=5000000000ULL
        SYSCAPE_SAILFISH_NAME_MAX=0
        SYSCAPE_SAILFISH_PATH_MAX=0
        SYSCAPE_SAILFISH_PROCESS_COUNT=0
        SYSCAPE_SAILFISH_SYSTEM_THREAD_COUNT=0
        SYSCAPE_SAILFISH_MAX_FILES=0
        SYSCAPE_SAILFISH_UPTIME_MS=-1
        SYSCAPE_SAILFISH_TASK_PRIORITY=5000000000ULL)

    syscape_add_test(
        sailfish_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(sailfish_odr PRIVATE SYSCAPE_TARGET_SAILFISH=1)

    foreach(sailfish_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(sailfish_header_target "header_${sailfish_domain}_sailfish")
        syscape_add_test(
            ${sailfish_header_target}
            17
            syscape::hosted
            header_${sailfish_domain}.cpp)
        target_compile_definitions(${sailfish_header_target} PRIVATE SYSCAPE_TARGET_SAILFISH=1)
    endforeach()

    # KaiOS tests on POSIX host using SYSCAPE_TARGET_KAIOS=1
    syscape_add_test(kaios_functional 17 syscape::hosted kaios_functional.cpp)
    target_compile_definitions(kaios_functional PRIVATE SYSCAPE_TARGET_KAIOS=1)

    syscape_add_test(kaios_overrides 17 syscape::hosted kaios_overrides.cpp)
    target_compile_definitions(kaios_overrides PRIVATE
        SYSCAPE_TARGET_KAIOS=1
        SYSCAPE_KAIOS_VERSION="3.0.0"
        SYSCAPE_KAIOS_KERNEL_NAME="Linux-custom"
        SYSCAPE_KAIOS_KERNEL_VERSION="5.4.0"
        SYSCAPE_KAIOS_HOST_NAME="my-kaios-device"
        SYSCAPE_KAIOS_UPTIME_MS=123456
        SYSCAPE_KAIOS_CPU_COUNT=8
        SYSCAPE_KAIOS_PHYSICAL_CPU_COUNT=4
        SYSCAPE_KAIOS_PACKAGE_COUNT=1
        SYSCAPE_KAIOS_PAGE_SIZE_BYTES=4096
        SYSCAPE_KAIOS_TOTAL_RAM_BYTES=4294967296ULL
        SYSCAPE_KAIOS_FREE_RAM_BYTES=2147483648ULL
        SYSCAPE_KAIOS_PID=100
        SYSCAPE_KAIOS_PPID=1
        SYSCAPE_KAIOS_THREAD_COUNT=16
        SYSCAPE_KAIOS_PRIORITY=5
        SYSCAPE_KAIOS_NAME_MAX=128
        SYSCAPE_KAIOS_PATH_MAX=2048
        SYSCAPE_KAIOS_PROCESS_COUNT=42
        SYSCAPE_KAIOS_SYSTEM_THREAD_COUNT=128
        SYSCAPE_KAIOS_MAX_FILES=1024)

    syscape_add_test(kaios_invalid_overrides 17 syscape::hosted kaios_invalid_overrides.cpp)
    target_compile_definitions(kaios_invalid_overrides PRIVATE
        SYSCAPE_TARGET_KAIOS=1
        SYSCAPE_KAIOS_CPU_COUNT=0
        SYSCAPE_KAIOS_PHYSICAL_CPU_COUNT=5000000000ULL
        SYSCAPE_KAIOS_PACKAGE_COUNT=0
        SYSCAPE_KAIOS_TOTAL_RAM_BYTES=0ULL
        SYSCAPE_KAIOS_FREE_RAM_BYTES=-1
        SYSCAPE_KAIOS_PAGE_SIZE_BYTES=0
        SYSCAPE_KAIOS_PID=0
        SYSCAPE_KAIOS_PPID=-1
        SYSCAPE_KAIOS_THREAD_COUNT=5000000000ULL
        SYSCAPE_KAIOS_NAME_MAX=0
        SYSCAPE_KAIOS_PATH_MAX=0
        SYSCAPE_KAIOS_PROCESS_COUNT=0
        SYSCAPE_KAIOS_SYSTEM_THREAD_COUNT=0
        SYSCAPE_KAIOS_MAX_FILES=0
        SYSCAPE_KAIOS_UPTIME_MS=-1
        SYSCAPE_KAIOS_TASK_PRIORITY=5000000000ULL)

    syscape_add_test(
        kaios_odr
        17
        syscape::hosted
        odr_main.cpp
        odr_other.cpp)
    target_compile_definitions(kaios_odr PRIVATE SYSCAPE_TARGET_KAIOS=1)

    foreach(kaios_domain IN ITEMS
            os cpu memory process user filesystem network locale environment
            resource virtualization hardware power sensor security numa
            process_list printer audio software storage ipc connection
            gpu display input camera bluetooth wifi)
        set(kaios_header_target "header_${kaios_domain}_kaios")
        syscape_add_test(
            ${kaios_header_target}
            17
            syscape::hosted
            header_${kaios_domain}.cpp)
        target_compile_definitions(${kaios_header_target} PRIVATE SYSCAPE_TARGET_KAIOS=1)
    endforeach()
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
    if(DEFINED CMAKE_CXX_STANDARD AND CMAKE_CXX_STANDARD GREATER 11)
        return()
    endif()
    if(INTEGRITY OR CMAKE_SYSTEM_NAME STREQUAL "INTEGRITY")
        return()
    endif()
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
