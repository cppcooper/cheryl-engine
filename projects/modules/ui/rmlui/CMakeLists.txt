cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/CherylModules.cmake")
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cheryl_ui_rmlui_module VERSION ${VERSION_PROJECT_MODULE_UI_RMLUI} LANGUAGES CXX)
endif()
if(TARGET Cheryl::UI::RmlUi)
    return()
endif()
cheryl_configure_module_options()
cheryl_prepare_module()
cheryl_configure_rmlui_options()

if(NOT TARGET RmlUi::Core)
    if(NOT CHERYL_RMLUI_SOURCE AND NOT RmlUi_DIR AND EXISTS "${CHERYL_REPOSITORY_ROOT}/extern/rmlui/CMakeLists.txt")
        set(CHERYL_RMLUI_SOURCE "${CHERYL_REPOSITORY_ROOT}/extern/rmlui")
    endif()
    if(CHERYL_RMLUI_SOURCE)
        include(cmake/RmlUi.cmake)
        cheryl_add_rmlui_source()
    else()
        find_package(RmlUi 6.3 EXACT CONFIG REQUIRED GLOBAL)
    endif()
endif()
get_target_property(rmlui_engine_services RmlUi::Core CHERYL_RMLUI_ENGINE_SERVICES)
if(NOT rmlui_engine_services)
    message(FATAL_ERROR
        "A supplied RmlUi Core target must omit its native font engine and declare CHERYL_RMLUI_ENGINE_SERVICES=TRUE. "
        "Use CHERYL_RMLUI_SOURCE for an owned compatible build.")
endif()
# Owned source marks its Core target after applying the correction. Supplied
# libraries require that provider declaration.
get_target_property(placeholder_fixed RmlUi::Core CHERYL_RMLUI_PLACEHOLDER_FIX)
if(NOT placeholder_fixed)
    message(FATAL_ERROR
        "RmlUi 6.3 Core must include the placeholder hit-testing correction. "
        "Use CHERYL_RMLUI_SOURCE for a Cheryl-patched source build or supply a Core target with "
        "CHERYL_RMLUI_PLACEHOLDER_FIX=TRUE after verifying the library includes the correction.")
endif()

get_target_property(rmlui_rasterizer_abi RmlUi::Core CHERYL_RMLUI_RASTERIZER_ABI)
if(NOT rmlui_rasterizer_abi STREQUAL "1")
    message(FATAL_ERROR "A supplied RmlUi shared runtime must provide CHERYL_RMLUI_RASTERIZER_ABI=1")
endif()

# Retain the reviewed toolkit provider, atlas, fallback and effect behavior while
# supplying its native font operations through the shared Engine service.
if(CHERYL_RMLUI_SOURCE)
    set(cheryl_rmlui_font_provider "${CHERYL_RMLUI_SOURCE}/Source/Core/FontEngineDefault")
else()
    set(cheryl_rmlui_font_provider "${CHERYL_REPOSITORY_ROOT}/extern/rmlui/Source/Core/FontEngineDefault")
endif()
if(NOT EXISTS "${cheryl_rmlui_font_provider}/FontEngineInterfaceDefault.cpp")
    message(FATAL_ERROR "The reviewed RmlUi font integration sources are required; supply CHERYL_RMLUI_SOURCE")
endif()

# Private adapter types use the selected Core target's public header tree, also
# when that target supplies installed headers separate from this source checkout.
set(cheryl_rmlui_font_source "${CMAKE_CURRENT_BINARY_DIR}/rmlui-font-headers")
foreach(header IN ITEMS FontTypes.h FreeTypeInterface.h)
    set(input "${cheryl_rmlui_font_provider}/${header}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input}")
    file(READ "${input}" contents)
    string(REGEX REPLACE "#include \"\\.\\./\\.\\./\\.\\./Include/(RmlUi/[^\"]+)\"" "#include <\\1>" contents "${contents}")
    file(CONFIGURE OUTPUT "${cheryl_rmlui_font_source}/${header}" CONTENT "${contents}" @ONLY)
endforeach()

file(GLOB_RECURSE cheryl_rmlui_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(${TARGET_LIB_MODULE_UI_RMLUI} ${CHERYL_LIBRARY_TYPE} ${cheryl_rmlui_sources})
add_library(Cheryl::UI::RmlUi ALIAS ${TARGET_LIB_MODULE_UI_RMLUI})
cheryl_register_public_component(${TARGET_LIB_MODULE_UI_RMLUI} FEATURE CHERYL_HAS_UI_RMLUI)
set_target_properties(${TARGET_LIB_MODULE_UI_RMLUI} PROPERTIES
    VERSION "${VERSION_PROJECT_MODULE_UI_RMLUI}"
    OUTPUT_NAME "${OUTPUT_LIB_MODULE_UI_RMLUI}"
    ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
cheryl_configure_library(${TARGET_LIB_MODULE_UI_RMLUI} UI_RMLUI)
get_target_property(rmlui_owned RmlUi::Core CHERYL_OWNED_UI_DEPENDENCY)
if(rmlui_owned)
    cheryl_shared_dependency(RmlUi::Core OWNER ${TARGET_LIB_MODULE_UI_RMLUI} OWNED)
else()
    cheryl_shared_dependency(RmlUi::Core OWNER ${TARGET_LIB_MODULE_UI_RMLUI})
endif()
target_include_directories(${TARGET_LIB_MODULE_UI_RMLUI} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cheryl>")
target_include_directories(${TARGET_LIB_MODULE_UI_RMLUI} PRIVATE "${cheryl_rmlui_font_source}")
target_include_directories(${TARGET_LIB_MODULE_UI_RMLUI} PUBLIC
    "$<BUILD_INTERFACE:$<TARGET_PROPERTY:RmlUi::Core,INTERFACE_INCLUDE_DIRECTORIES>>")
target_link_libraries(${TARGET_LIB_MODULE_UI_RMLUI} PUBLIC Cheryl::Engine RmlUi::Core)

if(CHERYL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(CHERYL_BUILD_CONSUMER_TESTS)
    add_subdirectory(tests/consumer)
endif()
