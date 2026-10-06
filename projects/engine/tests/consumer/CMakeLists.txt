cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cheryl_consumer LANGUAGES CXX)
endif()

if(NOT TARGET Cheryl::Engine)
    block(SCOPE_FOR VARIABLES)
        set(CHERYL_BUILD_TESTS OFF)
        set(CHERYL_BUILD_DEMO OFF)
        set(CHERYL_BUILD_CONSUMER_TESTS OFF)
        set(CHERYL_BUILD_NATIVE_GLFW OFF)
        set(CHERYL_BUILD_OPENGL OFF)
        set(CHERYL_BUILD_UI_TGUI OFF)
        add_subdirectory("${CHERYL_REPOSITORY_ROOT}" engine EXCLUDE_FROM_ALL)
    endblock()
endif()

include("${CHERYL_REPOSITORY_ROOT}/cmake/CherylConsumers.cmake")

# No dependency list, include directories, language standard or engine sources.
file(GLOB_RECURSE consumer_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_executable(${TARGET_TEST_CONSUMER_ENGINE} ${consumer_sources})
set_target_properties(${TARGET_TEST_CONSUMER_ENGINE} PROPERTIES OUTPUT_NAME "${OUTPUT_TEST_CONSUMER_ENGINE}")
if(DEFINED CHERYL_BUILD_ROOT)
    set_target_properties(${TARGET_TEST_CONSUMER_ENGINE} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
endif()
target_link_libraries(${TARGET_TEST_CONSUMER_ENGINE} PRIVATE ${LINKAGE_PRIVATE_TEST_CONSUMER_ENGINE})

add_subdirectory(headers)
add_dependencies(${TARGET_TEST_CONSUMER_ENGINE} ${TARGET_TEST_CONSUMER_ENGINE_HEADERS})
