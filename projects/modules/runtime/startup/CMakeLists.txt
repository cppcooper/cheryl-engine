cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/CherylModules.cmake")
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cheryl_startup_module VERSION ${VERSION_PROJECT_MODULE_STARTUP} LANGUAGES C CXX)
endif()
if(TARGET Cheryl::Startup)
    return()
endif()
cheryl_configure_module_options()
cheryl_configure_startup_options()
cheryl_prepare_module()

if(NOT TARGET CLI11::CLI11)
    add_subdirectory("${CHERYL_REPOSITORY_ROOT}/extern/cli11" extern/cli11 EXCLUDE_FROM_ALL)
endif()

if(CHERYL_STARTUP_OPENGL AND NOT TARGET Cheryl::OpenGL)
    if(NOT CHERYL_OPENGL_SOURCE OR NOT EXISTS "${CHERYL_OPENGL_SOURCE}/CMakeLists.txt")
        message(FATAL_ERROR "Configured OpenGL startup requires Cheryl::OpenGL or CHERYL_OPENGL_SOURCE")
    endif()
    block(SCOPE_FOR VARIABLES)
        set(CHERYL_BUILD_TESTS OFF)
        set(CHERYL_BUILD_CONSUMER_TESTS OFF)
        add_subdirectory("${CHERYL_OPENGL_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/opengl" EXCLUDE_FROM_ALL)
    endblock()
endif()

add_library(${TARGET_LIB_MODULE_STARTUP} ${CHERYL_LIBRARY_TYPE} src/startup.cpp src/defaults.cpp)
add_library(Cheryl::Startup ALIAS ${TARGET_LIB_MODULE_STARTUP})
cheryl_register_public_component(${TARGET_LIB_MODULE_STARTUP} FEATURE CHERYL_HAS_STARTUP)
set_target_properties(${TARGET_LIB_MODULE_STARTUP} PROPERTIES
    VERSION "${VERSION_PROJECT_MODULE_STARTUP}"
    OUTPUT_NAME "${OUTPUT_LIB_MODULE_STARTUP}"
    ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}"
    CHERYL_STARTUP_OPENGL "${CHERYL_STARTUP_OPENGL}")
cheryl_configure_library(${TARGET_LIB_MODULE_STARTUP} STARTUP)
target_compile_definitions(${TARGET_LIB_MODULE_STARTUP} PUBLIC CHERYL_HAS_SUPPORT_STARTUP=1)
target_compile_definitions(${TARGET_LIB_MODULE_STARTUP} PRIVATE CHERYL_STARTUP_OPENGL=$<BOOL:${CHERYL_STARTUP_OPENGL}>)
target_include_directories(${TARGET_LIB_MODULE_STARTUP} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cheryl>")
# CLI11 remains the explicit public option-registration frontend; it owns no SDK runtime binary.
target_link_libraries(${TARGET_LIB_MODULE_STARTUP} PUBLIC Cheryl::Engine CLI11::CLI11)
if(CHERYL_STARTUP_OPENGL)
    target_sources(${TARGET_LIB_MODULE_STARTUP} PRIVATE src/opengl-integration.cpp)
    target_link_libraries(${TARGET_LIB_MODULE_STARTUP} PRIVATE Cheryl::OpenGL)
    target_compile_definitions(${TARGET_LIB_MODULE_STARTUP} INTERFACE CHERYL_HAS_STARTUP_OPENGL=1)
    # Preserve the concrete source/SDK interface without another implementation library.
    add_library(${TARGET_LIB_MODULE_OPENGL_S_STARTUP} INTERFACE)
    add_library(Cheryl::OpenGL::Startup ALIAS ${TARGET_LIB_MODULE_OPENGL_S_STARTUP})
    target_link_libraries(${TARGET_LIB_MODULE_OPENGL_S_STARTUP} INTERFACE Cheryl::Startup Cheryl::OpenGL)
    target_compile_definitions(${TARGET_LIB_MODULE_OPENGL_S_STARTUP} INTERFACE CHERYL_HAS_SUPPORT_OPENGL_STARTUP=1)
    cheryl_register_public_component(${TARGET_LIB_MODULE_OPENGL_S_STARTUP})
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cheryl_select_public_component(${TARGET_LIB_MODULE_STARTUP})
endif()
if(CHERYL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(CHERYL_BUILD_CONSUMER_TESTS)
    add_subdirectory(tests/consumer)
endif()
