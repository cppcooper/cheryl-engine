cmake_minimum_required(VERSION 3.28)
project(cheryl_map_storage_u1 LANGUAGES CXX)

# Standalone CPU boundary: compile the Engine-owned source without its root build.
get_filename_component(map_storage_root "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
include("${map_storage_root}/projects/engine/cmake/MapStorageCpu.cmake")
option(MAP_STORAGE_BUILD_TESTS "Build the pinned gtest and Python process checks" ON)

add_executable(map_storage_cli cli.cpp)
target_link_libraries(map_storage_cli PRIVATE Cheryl::MapStorageCPU)
set_target_properties(map_storage_cli PROPERTIES CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(map_storage_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(NOT MAP_STORAGE_BUILD_TESTS)
    return()
endif()
if(NOT EXISTS "${map_storage_root}/extern/googletest/CMakeLists.txt")
    message(FATAL_ERROR "Missing pinned extern/googletest/CMakeLists.txt")
endif()
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory("${map_storage_root}/extern/googletest" "${CMAKE_CURRENT_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)

# Private application prototype; the public codec/file implementation is reused unchanged.
add_library(map_storage_patches STATIC cell-patch.cpp)
target_link_libraries(map_storage_patches PUBLIC Cheryl::MapStorageCPU)
set_target_properties(map_storage_patches PROPERTIES CXX_EXTENSIONS OFF)
add_executable(map_storage_patch_consumer patch-consumer.cpp)
target_link_libraries(map_storage_patch_consumer PRIVATE map_storage_patches)
set_target_properties(map_storage_patch_consumer PROPERTIES CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(map_storage_patches PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(map_storage_patch_consumer PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(map_storage_tests codec-tests.cpp file-storage-tests.cpp cli-diagnostics-tests.cpp cell-patch-tests.cpp validation-tests.cpp)
target_link_libraries(map_storage_tests PRIVATE map_storage_patches GTest::gtest_main)
# Only fault tests may see the private native seam; it is never a usage requirement.
target_include_directories(map_storage_tests PRIVATE "${map_storage_root}/projects/engine/src/core/resources/map-storage")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(map_storage_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
enable_testing()
include(GoogleTest)
gtest_discover_tests(map_storage_tests TEST_PREFIX "map_storage." PROPERTIES LABELS "map_storage" DISCOVERY_MODE PRE_TEST)

# Test tooling only; the CLI itself needs only the codec/adapter and C++ runtime.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    foreach(map_storage_case IN ITEMS round_trip preservation)
        add_test(NAME "map_storage.patch_process.${map_storage_case}"
            COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:map_storage_patch_consumer>"
                "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/patch-checks" "-DMODE=${map_storage_case}"
                -P "${map_storage_root}/projects/engine/tests/map-storage-consumer/check.cmake")
        set_tests_properties("map_storage.patch_process.${map_storage_case}" PROPERTIES LABELS "map_storage")
    endforeach()
    foreach(map_storage_case IN ITEMS round_trip comparison malformed truncated format_version missing profiles
                                     arguments help native_path failed_write_retry output_failure)
        add_test(NAME "map_storage.cli.${map_storage_case}"
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/cli-checks.py"
                --cli "$<TARGET_FILE:map_storage_cli>" --case "${map_storage_case}"
                --work-root "${CMAKE_CURRENT_BINARY_DIR}/cli-checks")
        set_tests_properties("map_storage.cli.${map_storage_case}" PROPERTIES LABELS "map_storage")
    endforeach()
endif()
