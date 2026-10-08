cmake_minimum_required(VERSION 3.28)

include("${CHERYL_REPOSITORY_ROOT}/cmake/CherylModules.cmake")
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cheryl_opengl_module VERSION ${VERSION_PROJECT_MODULE_OPENGL} LANGUAGES C CXX)
endif()
if(TARGET Cheryl::OpenGL)
    return()
endif()
cheryl_option(CHERYL_BUILD_TESTS "Build module-local checks" OFF)
cheryl_prepare_module(REQUIRES_NATIVE)
get_target_property(CHERYL_NATIVE_INPUT Cheryl::NativeGLFW CHERYL_NATIVE_INPUT)

if(NOT TARGET OpenGL::GL)
    find_package(OpenGL REQUIRED)
endif()
if(NOT TARGET ${TARGET_LIB_MODULE_OPENGL_S_GL46})
    if(NOT COMMAND glad_add_library)
        add_subdirectory("${CHERYL_REPOSITORY_ROOT}/extern/glad/cmake" extern/glad/cmake)
    endif()
    add_subdirectory(support/gl46)
endif()

file(GLOB_RECURSE cheryl_opengl_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(${TARGET_LIB_MODULE_OPENGL} STATIC ${cheryl_opengl_sources})
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

add_subdirectory(support/startup EXCLUDE_FROM_ALL)

if(CHERYL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(CHERYL_BUILD_CONSUMER_TESTS)
    add_subdirectory(tests/consumer)
endif()
