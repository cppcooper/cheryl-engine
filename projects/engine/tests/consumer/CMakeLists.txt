cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cheryl_consumer LANGUAGES CXX)
endif()

if(NOT TARGET Cheryl::Engine)
    get_filename_component(cheryl_default_engine_source "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
    set(CHERYL_ENGINE_SOURCE "${cheryl_default_engine_source}" CACHE PATH "Cheryl source checkout")
    set(CHERYL_BUILD_TESTS OFF CACHE BOOL "Build Cheryl's tests and acceptance tools")
    set(CHERYL_BUILD_DEMO OFF CACHE BOOL "Build the windowed demonstration application")
    set(CHERYL_BUILD_CONSUMER_TESTS OFF)
    add_subdirectory("${CHERYL_ENGINE_SOURCE}" engine EXCLUDE_FROM_ALL)
endif()

# No dependency list, include directories, language standard or engine sources.
add_executable(cheryl-consumer src/main.cpp src/resources.cpp)
if(DEFINED CHERYL_BUILD_ROOT)
    set_target_properties(cheryl-consumer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
endif()
target_link_libraries(cheryl-consumer PRIVATE Cheryl::Engine)

add_subdirectory(headers)
add_dependencies(cheryl-consumer cheryl-consumer-headers)
