cmake_minimum_required(VERSION 3.28)

if(CHERYL_ENGINE_SOURCE)
    include("${CHERYL_ENGINE_SOURCE}/cmake/CherylModules.cmake")
else()
    include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/CherylModules.cmake")
endif()
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cheryl_opengl_module VERSION ${VERSION_PROJECT_MODULE_OPENGL} LANGUAGES C CXX)
endif()
if(TARGET Cheryl::OpenGL)
    return()
endif()
option(CHERYL_BUILD_TESTS "Build module-local checks" OFF)
cheryl_prepare_module(REQUIRES_NATIVE)
get_target_property(CHERYL_NATIVE_INPUT Cheryl::NativeGLFW CHERYL_NATIVE_INPUT)

if(NOT TARGET OpenGL::GL)
    find_package(OpenGL REQUIRED)
endif()
if(NOT TARGET ${TARGET_LIB_MODULE_OPENGL_GL46})
    if(NOT COMMAND glad_add_library)
        add_subdirectory("${CHERYL_REPOSITORY_ROOT}/extern/glad/cmake" extern/glad/cmake)
    endif()
    add_subdirectory(support/gl46)
endif()

add_library(${TARGET_LIB_MODULE_OPENGL} STATIC
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
add_library(Cheryl::OpenGL ALIAS ${TARGET_LIB_MODULE_OPENGL})
set_target_properties(${TARGET_LIB_MODULE_OPENGL} PROPERTIES
    VERSION "${VERSION_PROJECT_MODULE_OPENGL}"
    OUTPUT_NAME "${OUTPUT_LIB_MODULE_OPENGL}"
    ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_include_directories(${TARGET_LIB_MODULE_OPENGL} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cheryl>")
target_link_libraries(${TARGET_LIB_MODULE_OPENGL}
    PUBLIC ${LINKAGE_PUBLIC_LIB_MODULE_OPENGL}
    PRIVATE ${LINKAGE_PRIVATE_LIB_MODULE_OPENGL})

if(CHERYL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(CHERYL_BUILD_CONSUMER_TESTS)
    add_subdirectory(tests/consumer)
endif()
