cmake_minimum_required(VERSION 3.28)

include("${CHERYL_REPOSITORY_ROOT}/cmake/CherylModules.cmake")
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cheryl_ui_rmlui_module VERSION ${VERSION_PROJECT_MODULE_UI_RMLUI} LANGUAGES CXX)
endif()
if(TARGET Cheryl::UI::RmlUi)
    return()
endif()
cheryl_option(CHERYL_BUILD_TESTS "Build module-local checks" OFF)
cheryl_prepare_module()
cheryl_option(CHERYL_RMLUI_PLACEHOLDER_FIX_VERIFIED
    "Supplied RmlUi Core was verified to include the placeholder hit-testing correction" OFF)

if(NOT TARGET RmlUi::Core)
    if(NOT CHERYL_RMLUI_SOURCE AND NOT RmlUi_DIR AND EXISTS "${CHERYL_REPOSITORY_ROOT}/extern/rmlui/CMakeLists.txt")
        set(CHERYL_RMLUI_SOURCE "${CHERYL_REPOSITORY_ROOT}/extern/rmlui")
    endif()
    if(CHERYL_RMLUI_SOURCE)
        include(cmake/RmlUi.cmake)
        cheryl_add_rmlui_source()
    else()
        find_package(RmlUi 6.3 EXACT CONFIG REQUIRED)
    endif()
endif()
if(NOT RMLUI_FONT_ENGINE STREQUAL "freetype")
    message(FATAL_ERROR "Cheryl UI requires RMLUI_FONT_ENGINE=freetype; a supplied Core target must declare this configuration")
endif()
# Owned source marks its Core target after applying the correction. Supplied
# libraries require that provider declaration or explicit consumer verification.
get_target_property(placeholder_fixed RmlUi::Core CHERYL_RMLUI_PLACEHOLDER_FIX)
if(NOT placeholder_fixed AND NOT CHERYL_RMLUI_PLACEHOLDER_FIX_VERIFIED)
    message(FATAL_ERROR
        "RmlUi 6.3 Core must include the placeholder hit-testing correction. "
        "Use CHERYL_RMLUI_SOURCE for a Cheryl-patched source build, supply a Core target with "
        "CHERYL_RMLUI_PLACEHOLDER_FIX=TRUE, or set CHERYL_RMLUI_PLACEHOLDER_FIX_VERIFIED=ON "
        "only after verifying the supplied library includes the correction.")
endif()

file(GLOB_RECURSE cheryl_rmlui_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(${TARGET_LIB_MODULE_UI_RMLUI} STATIC ${cheryl_rmlui_sources})
add_library(Cheryl::UI::RmlUi ALIAS ${TARGET_LIB_MODULE_UI_RMLUI})
set_target_properties(${TARGET_LIB_MODULE_UI_RMLUI} PROPERTIES
    VERSION "${VERSION_PROJECT_MODULE_UI_RMLUI}"
    OUTPUT_NAME "${OUTPUT_LIB_MODULE_UI_RMLUI}"
    ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_include_directories(${TARGET_LIB_MODULE_UI_RMLUI} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cheryl>")
target_link_libraries(${TARGET_LIB_MODULE_UI_RMLUI} PUBLIC ${LINKAGE_PUBLIC_LIB_MODULE_UI_RMLUI})

if(CHERYL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(CHERYL_BUILD_CONSUMER_TESTS)
    add_subdirectory(tests/consumer)
endif()
