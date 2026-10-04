cmake_minimum_required(VERSION 3.28)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cheryl_native_glfw_module LANGUAGES C CXX)
endif()
if(TARGET Cheryl::NativeGLFW)
    return()
endif()
if(NOT COMMAND cheryl_prepare_module)
    if(CHERYL_ENGINE_SOURCE)
        include("${CHERYL_ENGINE_SOURCE}/cmake/CherylModules.cmake")
    else()
        include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/CherylModules.cmake")
    endif()
endif()
option(CHERYL_BUILD_TESTS "Build module-local checks" OFF)
cheryl_prepare_module()

set(cheryl_native_input_default ON)
if(CHERYL_SANDBOX_BUILD)
    set(cheryl_native_input_default OFF)
    message(DEPRECATION "CHERYL_SANDBOX_BUILD is replaced by CHERYL_NATIVE_NULL_PLATFORM=ON and CHERYL_NATIVE_INPUT=OFF")
endif()
option(CHERYL_NATIVE_INPUT "Include the Gainput-backed native input adapter" ${cheryl_native_input_default})
set(cheryl_native_null_default OFF)
if(CHERYL_SANDBOX_BUILD)
    set(cheryl_native_null_default ON)
endif()
option(CHERYL_NATIVE_NULL_PLATFORM "Use GLFW's null platform without X11 or Wayland" ${cheryl_native_null_default})

if(NOT TARGET glfw)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    if(CHERYL_NATIVE_NULL_PLATFORM)
        set(GLFW_BUILD_X11 OFF)
        set(GLFW_BUILD_WAYLAND OFF)
    endif()
    add_subdirectory("${CHERYL_REPOSITORY_ROOT}/extern/glfw" extern/glfw EXCLUDE_FROM_ALL)
endif()

set(cheryl_native_sources
    src/core/display/display-system.cpp
    src/core/display/glfw-diagnostics.cpp
    src/core/display/window.cpp)
if(CHERYL_NATIVE_INPUT)
    list(APPEND cheryl_native_sources
        src/core/controls/glfw-bindings.cpp
        src/core/controls/input-mapper.cpp
        src/core/controls/input-system.cpp)
    if(UNIX AND NOT APPLE AND NOT TARGET X11::X11)
        find_package(X11 REQUIRED)
    endif()
    if(NOT TARGET gainputstatic)
        add_subdirectory("${CHERYL_REPOSITORY_ROOT}/extern/gainput" extern/gainput EXCLUDE_FROM_ALL)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(gainputstatic PRIVATE -Wno-implicit-fallthrough -Wno-unused-parameter)
        endif()
    endif()
endif()

add_library(cheryl_native_glfw STATIC ${cheryl_native_sources})
add_library(Cheryl::NativeGLFW ALIAS cheryl_native_glfw)
set_target_properties(cheryl_native_glfw PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}"
    CHERYL_NATIVE_INPUT "${CHERYL_NATIVE_INPUT}")
target_include_directories(cheryl_native_glfw PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cheryl>")
target_compile_definitions(cheryl_native_glfw PUBLIC CHERYL_NATIVE_INPUT=$<BOOL:${CHERYL_NATIVE_INPUT}>)
target_link_libraries(cheryl_native_glfw PUBLIC Cheryl::Engine PRIVATE glfw)
if(CHERYL_NATIVE_INPUT)
    # Gainput's public types appear in the native input headers.
    target_include_directories(cheryl_native_glfw SYSTEM PUBLIC
        "$<BUILD_INTERFACE:${CHERYL_REPOSITORY_ROOT}/extern/gainput/lib/include>")
    target_link_libraries(cheryl_native_glfw PUBLIC gainputstatic)
    if(UNIX AND NOT APPLE)
        target_link_libraries(cheryl_native_glfw PRIVATE X11::X11)
    endif()
endif()

if(CHERYL_BUILD_TESTS)
    add_subdirectory(tests)
endif()
if(CHERYL_BUILD_CONSUMER_TESTS)
    add_subdirectory(tests/consumer)
endif()
