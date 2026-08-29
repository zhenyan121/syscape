function(syscape_add_example target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE syscape::hosted)
    set_target_properties(
        ${target}
        PROPERTIES
            CXX_STANDARD 17
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

    if(WIN32)
        target_link_libraries(
            ${target}
            PRIVATE
                advapi32
                psapi
                iphlpapi
                setupapi
                wtsapi32
                powrprof
                bthprops
                wlanapi
                winspool
                ole32
                uuid)
    endif()

    if(APPLE)
        target_link_libraries(
            ${target}
            PRIVATE
                "-framework CoreFoundation"
                "-framework IOKit"
                "-framework SystemConfiguration"
                "-framework CoreGraphics"
                "-framework CoreAudio"
                "-framework CoreMediaIO"
                "-framework DiskArbitration")
    endif()
endfunction()

syscape_add_example(example_system_overview system_overview.cpp)
syscape_add_example(example_hardware_inventory hardware_inventory.cpp)
syscape_add_example(example_network_overview network_overview.cpp)
