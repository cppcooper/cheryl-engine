cmake_minimum_required(VERSION 3.28)
project(cheryl_map_storage_consumer LANGUAGES CXX)

# Exercise the standalone entry's tests-off path, with no gtest/Python discovery.
# The executable and probes consume only its declared production link interface.
option(MAP_STORAGE_BUILD_TESTS "Build the pinned gtest and Python process checks" OFF)
if(MAP_STORAGE_BUILD_TESTS)
    message(FATAL_ERROR "This independent CPU consumer requires MAP_STORAGE_BUILD_TESTS=OFF")
endif()
get_filename_component(map_storage_repository "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
add_subdirectory("${map_storage_repository}/projects/tests/map-storage" boundary EXCLUDE_FROM_ALL)

add_executable(map_storage_consumer main.cpp)
target_link_libraries(map_storage_consumer PRIVATE Cheryl::MapStorageCPU)
set_target_properties(map_storage_consumer PROPERTIES CXX_EXTENSIONS OFF)

# No prefix/umbrella header or consumer-supplied visibility definition. Each
# public header is first and repeated, then instantiated using its own contract.
set(map_storage_probe_sources)
foreach(map_storage_header IN ITEMS snapshot codec file-storage)
    set(map_storage_probe "${CMAKE_CURRENT_BINARY_DIR}/${map_storage_header}-first.cpp")
    file(WRITE "${map_storage_probe}" "#include <cheryl/core/resources/map-storage/${map_storage_header}.h>\n#include <cheryl/core/resources/map-storage/${map_storage_header}.h>\n#ifndef CE_ENGINE_STATIC\n#error The linked CPU target must declare static Engine visibility\n#endif\n")
    if(map_storage_header STREQUAL "snapshot")
        file(APPEND "${map_storage_probe}" "static_assert(CE::Maps::Snapshot{} == CE::Maps::Snapshot{});\n")
    else()
        file(APPEND "${map_storage_probe}" "#include <string_view>\n#define PROBE_STRING_(value) #value\n#define PROBE_STRING(value) PROBE_STRING_(value)\nstatic_assert(std::string_view(PROBE_STRING(CE_API)).empty());\n")
    endif()
    list(APPEND map_storage_probe_sources "${map_storage_probe}")
endforeach()
add_library(map_storage_header_probes OBJECT ${map_storage_probe_sources})
target_link_libraries(map_storage_header_probes PRIVATE Cheryl::MapStorageCPU)
set_target_properties(map_storage_header_probes PROPERTIES CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(map_storage_consumer PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(map_storage_header_probes PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Only this small consumer's process checks remain enabled; no test dependency.
enable_testing()
foreach(map_storage_check IN ITEMS memory round_trip preservation)
    add_test(NAME "map_storage.consumer.${map_storage_check}"
        COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:map_storage_consumer>"
            "-DMODE=${map_storage_check}" "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/consumer-checks"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check.cmake")
    set_tests_properties("map_storage.consumer.${map_storage_check}" PROPERTIES LABELS "map_storage")
endforeach()
