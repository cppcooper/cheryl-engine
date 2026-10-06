cmake_minimum_required(VERSION 3.28)

if(CHERYL_ENGINE_SOURCE)
    include("${CHERYL_ENGINE_SOURCE}/cmake/CherylModules.cmake")
else()
    include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/CherylModules.cmake")
endif()
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cheryl_ui_tgui_module VERSION ${VERSION_PROJECT_MODULE_UI_TGUI} LANGUAGES CXX)
endif()
if(TARGET Cheryl::UI::TGUI)
    return()
endif()
option(CHERYL_BUILD_TESTS "Build module-local checks" OFF)
cheryl_prepare_module()

if(NOT TARGET TGUI::TGUI)
    if(NOT CHERYL_TGUI_SOURCE AND NOT TGUI_DIR AND EXISTS "${CHERYL_REPOSITORY_ROOT}/extern/tgui/CMakeLists.txt")
        set(CHERYL_TGUI_SOURCE "${CHERYL_REPOSITORY_ROOT}/extern/tgui")
    endif()
    if(CHERYL_TGUI_SOURCE)
        include(cmake/TGUI.cmake)
        cheryl_add_tgui_source()
    else()
        find_package(TGUI 1.13.0 EXACT CONFIG REQUIRED)
    endif()
endif()

file(GLOB_RECURSE cheryl_tgui_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(${TARGET_LIB_MODULE_UI_TGUI} STATIC ${cheryl_tgui_sources})
add_library(Cheryl::UI::TGUI ALIAS ${TARGET_LIB_MODULE_UI_TGUI})
set_target_properties(${TARGET_LIB_MODULE_UI_TGUI} PROPERTIES
    VERSION "${VERSION_PROJECT_MODULE_UI_TGUI}"
    OUTPUT_NAME "${OUTPUT_LIB_MODULE_UI_TGUI}"
    ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_include_directories(${TARGET_LIB_MODULE_UI_TGUI} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cheryl>")
target_link_libraries(${TARGET_LIB_MODULE_UI_TGUI} PUBLIC ${LINKAGE_PUBLIC_LIB_MODULE_UI_TGUI})

if(CHERYL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(CHERYL_BUILD_CONSUMER_TESTS)
    add_subdirectory(tests/consumer)
endif()
