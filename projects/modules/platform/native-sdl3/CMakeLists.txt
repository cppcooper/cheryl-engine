cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/CherylModules.cmake")
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cheryl_native_sdl3_scaffold LANGUAGES C CXX)
endif()
if(TARGET Cheryl::NativeSDL3::Scaffold)
    return()
endif()
cheryl_prepare_module()

# Establish the source/header boundary without registering an implemented SDK
# component or selecting a runtime factory before the SDL3 adapters exist.
add_library(module_native_sdl3_scaffold INTERFACE)
add_library(Cheryl::NativeSDL3::Scaffold ALIAS module_native_sdl3_scaffold)
target_link_libraries(module_native_sdl3_scaffold INTERFACE Cheryl::Engine)
target_include_directories(module_native_sdl3_scaffold INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/cheryl")
set_property(TARGET module_native_sdl3_scaffold PROPERTY CHERYL_SCAFFOLD TRUE)

if(CHERYL_BUILD_CONSUMER_TESTS)
    include("${CHERYL_REPOSITORY_ROOT}/cmake/CherylConsumers.cmake")
    cheryl_add_header_probes(consumer-headers-native-sdl3-scaffold Cheryl::NativeSDL3::Scaffold
        NEUTRAL HEADERS cheryl/backends/native-sdl3.h)
endif()
