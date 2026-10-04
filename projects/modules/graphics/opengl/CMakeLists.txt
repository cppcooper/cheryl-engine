cmake_minimum_required(VERSION 3.28)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cheryl_opengl_module LANGUAGES C CXX)
endif()
if(TARGET Cheryl::OpenGL)
    return()
endif()
if(NOT COMMAND cheryl_prepare_module)
    if(CHERYL_ENGINE_SOURCE)
        include("${CHERYL_ENGINE_SOURCE}/cmake/CherylModules.cmake")
    else()
        include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/CherylModules.cmake")
    endif()
endif()
option(CHERYL_BUILD_TESTS "Build module-local checks" OFF)
cheryl_prepare_module(REQUIRES_NATIVE)
get_target_property(CHERYL_NATIVE_INPUT Cheryl::NativeGLFW CHERYL_NATIVE_INPUT)

if(NOT TARGET OpenGL::GL)
    find_package(OpenGL REQUIRED)
endif()
if(NOT TARGET gl46)
    if(NOT COMMAND glad_add_library)
        add_subdirectory("${CHERYL_REPOSITORY_ROOT}/extern/glad/cmake" extern/glad/cmake)
    endif()
    add_subdirectory(support/gl46)
endif()

add_library(cheryl_opengl STATIC
    src/backends/opengl/debug-output.cpp
    src/backends/opengl/glfw-backend.cpp
    src/backends/opengl/glfw-context.cpp
    src/backends/opengl/glslprogram.cpp
    src/backends/opengl/pipeline.cpp
    src/backends/opengl/program-builder.cpp
    src/backends/opengl/renderer.cpp
    src/backends/opengl/resource-lifetime.cpp
    src/backends/opengl/resource-provider.cpp
    src/backends/opengl/texture.cpp
    src/backends/opengl/vertex-array-object.cpp)
add_library(Cheryl::OpenGL ALIAS cheryl_opengl)
set_target_properties(cheryl_opengl PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_include_directories(cheryl_opengl PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cheryl>")
target_link_libraries(cheryl_opengl
    PUBLIC Cheryl::Engine Cheryl::NativeGLFW gl46
    PRIVATE glfw OpenGL::GL)

if(CHERYL_BUILD_TESTS)
    add_subdirectory(tests)
endif()
if(CHERYL_BUILD_CONSUMER_TESTS)
    add_subdirectory(tests/consumer)
endif()
