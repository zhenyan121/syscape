cmake_minimum_required(VERSION 3.15)

project(syscape VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(syscape INTERFACE)
add_library(syscape::syscape ALIAS syscape)

target_compile_features(syscape INTERFACE cxx_std_11)
target_include_directories(
    syscape
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

add_library(syscape_hosted INTERFACE)
add_library(syscape::hosted ALIAS syscape_hosted)
target_link_libraries(syscape_hosted INTERFACE syscape::syscape)
target_compile_features(syscape_hosted INTERFACE cxx_std_17)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    option(SYSCAPE_BUILD_TESTS "Build Syscape tests" ON)
    option(SYSCAPE_BUILD_EXAMPLES "Build Syscape examples" ON)
else()
    option(SYSCAPE_BUILD_TESTS "Build Syscape tests" OFF)
    option(SYSCAPE_BUILD_EXAMPLES "Build Syscape examples" OFF)
endif()

if(SYSCAPE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(SYSCAPE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

install(
    TARGETS syscape syscape_hosted
    EXPORT syscapeTargets)

install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/syscapeConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/syscapeConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/syscapeConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/syscape)

install(
    EXPORT syscapeTargets
    FILE syscapeTargets.cmake
    NAMESPACE syscape::
    DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/syscape)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/syscapeConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/syscapeConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/syscape)

if(IS_ABSOLUTE "${CMAKE_INSTALL_DATADIR}")
    set(SYSCAPE_PKGCONFIG_PREFIX "${CMAKE_INSTALL_PREFIX}")
else()
    file(
        RELATIVE_PATH
        SYSCAPE_PKGCONFIG_PREFIX_FROM_FILE
        "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/pkgconfig"
        "${CMAKE_INSTALL_PREFIX}")
    string(
        REGEX REPLACE "/$" ""
        SYSCAPE_PKGCONFIG_PREFIX_FROM_FILE
        "${SYSCAPE_PKGCONFIG_PREFIX_FROM_FILE}")
    set(
        SYSCAPE_PKGCONFIG_PREFIX
        "\${pcfiledir}/${SYSCAPE_PKGCONFIG_PREFIX_FROM_FILE}")
endif()

if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(SYSCAPE_PKGCONFIG_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(
        SYSCAPE_PKGCONFIG_INCLUDEDIR
        "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/syscape.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/syscape.pc"
    @ONLY)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/syscape.pc"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/pkgconfig)
