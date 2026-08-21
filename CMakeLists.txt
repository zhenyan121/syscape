cmake_minimum_required(VERSION 3.15)

project(syscape VERSION 0.1.0 LANGUAGES CXX)

add_library(syscape INTERFACE)
add_library(syscape::syscape ALIAS syscape)

target_compile_features(syscape INTERFACE cxx_std_11)
target_include_directories(
    syscape
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

add_library(syscape_hosted INTERFACE)
add_library(syscape::hosted ALIAS syscape_hosted)
target_link_libraries(syscape_hosted INTERFACE syscape::syscape)
target_compile_features(syscape_hosted INTERFACE cxx_std_17)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    option(SYSCAPE_BUILD_TESTS "Build Syscape tests" ON)
else()
    option(SYSCAPE_BUILD_TESTS "Build Syscape tests" OFF)
endif()

if(SYSCAPE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
