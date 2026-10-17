cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/CherylModules.cmake")
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cheryl_opengl_module VERSION ${VERSION_PROJECT_MODULE_OPENGL} LANGUAGES C CXX)
endif()
if(TARGET Cheryl::OpenGL)
    return()
endif()
cheryl_configure_module_options()
cheryl_configure_opengl_options()
cheryl_prepare_module(REQUIRES_NATIVE)
get_target_property(CHERYL_NATIVE_INPUT Cheryl::NativeGLFW CHERYL_NATIVE_INPUT)

if(NOT TARGET OpenGL::GL)
    find_package(OpenGL REQUIRED GLOBAL)
endif()
set(cheryl_glad_owned OFF)
if(NOT TARGET ${TARGET_LIB_MODULE_OPENGL_S_GL46})
    if(NOT COMMAND glad_add_library)
        add_subdirectory("${CHERYL_REPOSITORY_ROOT}/extern/glad/cmake" extern/glad/cmake)
    endif()
    add_subdirectory(support/gl46)
    set(cheryl_glad_owned ON)
endif()

file(GLOB_RECURSE cheryl_opengl_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(${TARGET_LIB_MODULE_OPENGL} ${CHERYL_LIBRARY_TYPE} ${cheryl_opengl_sources})
add_library(Cheryl::OpenGL ALIAS ${TARGET_LIB_MODULE_OPENGL})
cheryl_register_public_component(${TARGET_LIB_MODULE_OPENGL} FEATURE CHERYL_HAS_OPENGL)
set_target_properties(${TARGET_LIB_MODULE_OPENGL} PROPERTIES
    VERSION "${VERSION_PROJECT_MODULE_OPENGL}"
    OUTPUT_NAME "${OUTPUT_LIB_MODULE_OPENGL}"
    ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
cheryl_configure_library(${TARGET_LIB_MODULE_OPENGL} OPENGL)
cheryl_shared_dependency(OpenGL::GL OWNER ${TARGET_LIB_MODULE_OPENGL} SYSTEM)
if(cheryl_glad_owned)
    cheryl_private_dependency(${TARGET_LIB_MODULE_OPENGL_S_GL46} OWNER ${TARGET_LIB_MODULE_OPENGL} OWNED)
else()
    cheryl_private_dependency(${TARGET_LIB_MODULE_OPENGL_S_GL46} OWNER ${TARGET_LIB_MODULE_OPENGL})
endif()
target_include_directories(${TARGET_LIB_MODULE_OPENGL} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cheryl>")
target_include_directories(${TARGET_LIB_MODULE_OPENGL} PRIVATE
    "${CHERYL_REPOSITORY_ROOT}/extern/json/single_include")
target_link_libraries(${TARGET_LIB_MODULE_OPENGL}
    PUBLIC ${LINKAGE_PUBLIC_LIB_MODULE_OPENGL}
    PRIVATE ${LINKAGE_PRIVATE_LIB_MODULE_OPENGL})

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cheryl_select_public_component(${TARGET_LIB_MODULE_OPENGL})
endif()

if(CHERYL_BUILD_TESTS)
    target_sources(${TARGET_LIB_MODULE_OPENGL} PRIVATE tests/support/src/gl-access.cpp)
    target_include_directories(${TARGET_LIB_MODULE_OPENGL} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    enable_testing()
    add_subdirectory(tests)
endif()
if(CHERYL_BUILD_CONSUMER_TESTS)
    add_subdirectory(tests/consumer)
endif()
