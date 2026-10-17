cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/CherylModules.cmake")
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cheryl_ui_tgui_module VERSION ${VERSION_PROJECT_MODULE_UI_TGUI} LANGUAGES CXX)
endif()
if(TARGET Cheryl::UI::TGUI)
    return()
endif()
cheryl_configure_module_options()
cheryl_prepare_module()
cheryl_configure_tgui_options()

if(NOT TARGET TGUI::TGUI)
    if(NOT CHERYL_TGUI_SOURCE AND NOT TGUI_DIR AND EXISTS "${CHERYL_REPOSITORY_ROOT}/extern/tgui/CMakeLists.txt")
        set(CHERYL_TGUI_SOURCE "${CHERYL_REPOSITORY_ROOT}/extern/tgui")
    endif()
    if(CHERYL_TGUI_SOURCE)
        include(cmake/TGUI.cmake)
        cheryl_add_tgui_source()
    else()
        find_package(TGUI 1.13.0 EXACT CONFIG REQUIRED GLOBAL)
    endif()
endif()

get_target_property(tgui_engine_services TGUI::TGUI CHERYL_TGUI_ENGINE_SERVICES)
if(NOT tgui_engine_services)
    message(FATAL_ERROR
        "A supplied TGUI target must use the custom Engine font/image services configuration and declare "
        "CHERYL_TGUI_ENGINE_SERVICES=TRUE. Use CHERYL_TGUI_SOURCE for an owned compatible build.")
endif()
get_target_property(tgui_decoder_abi TGUI::TGUI CHERYL_TGUI_IMAGE_DECODER_ABI)
if(NOT tgui_decoder_abi STREQUAL "1")
    message(FATAL_ERROR "A supplied TGUI shared runtime must provide CHERYL_TGUI_IMAGE_DECODER_ABI=1")
endif()

file(GLOB_RECURSE cheryl_tgui_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(${TARGET_LIB_MODULE_UI_TGUI} ${CHERYL_LIBRARY_TYPE} ${cheryl_tgui_sources})
add_library(Cheryl::UI::TGUI ALIAS ${TARGET_LIB_MODULE_UI_TGUI})
cheryl_register_public_component(${TARGET_LIB_MODULE_UI_TGUI} FEATURE CHERYL_HAS_UI_TGUI)
set_target_properties(${TARGET_LIB_MODULE_UI_TGUI} PROPERTIES
    VERSION "${VERSION_PROJECT_MODULE_UI_TGUI}"
    OUTPUT_NAME "${OUTPUT_LIB_MODULE_UI_TGUI}"
    ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
cheryl_configure_library(${TARGET_LIB_MODULE_UI_TGUI} UI_TGUI)
get_target_property(tgui_owned TGUI::TGUI CHERYL_OWNED_UI_DEPENDENCY)
if(tgui_owned)
    cheryl_shared_dependency(TGUI::TGUI OWNER ${TARGET_LIB_MODULE_UI_TGUI} OWNED)
else()
    cheryl_shared_dependency(TGUI::TGUI OWNER ${TARGET_LIB_MODULE_UI_TGUI})
endif()
target_include_directories(${TARGET_LIB_MODULE_UI_TGUI} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cheryl>")
target_include_directories(${TARGET_LIB_MODULE_UI_TGUI} PUBLIC
    "$<BUILD_INTERFACE:$<TARGET_PROPERTY:TGUI::TGUI,INTERFACE_INCLUDE_DIRECTORIES>>")
target_link_libraries(${TARGET_LIB_MODULE_UI_TGUI} PUBLIC Cheryl::Engine TGUI::TGUI)
# Native-only authors need the adapter's decoder registration even when no
# Session or other Cheryl symbol is referenced. Retain its C ABI anchor before
# static archive scanning and when linking with --as-needed or import libraries.
set(tgui_decoder_anchor cheryl_tgui_retain_image_loader)
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    string(PREPEND tgui_decoder_anchor "_")
endif()
if(MSVC)
    target_link_options(${TARGET_LIB_MODULE_UI_TGUI} INTERFACE "LINKER:/INCLUDE:${tgui_decoder_anchor}")
elseif(APPLE)
    target_link_options(${TARGET_LIB_MODULE_UI_TGUI} INTERFACE "LINKER:-u,_${tgui_decoder_anchor}")
else()
    target_link_options(${TARGET_LIB_MODULE_UI_TGUI} INTERFACE "LINKER:-u,${tgui_decoder_anchor}")
endif()

if(CHERYL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(CHERYL_BUILD_CONSUMER_TESTS)
    add_subdirectory(tests/consumer)
endif()
