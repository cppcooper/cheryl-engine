cmake_minimum_required(VERSION 3.28)
project(cheryl_map_storage_u1 LANGUAGES CXX)

# Standalone private boundary: do not include the Engine/root build or use its suite helper.
get_filename_component(map_storage_root "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
if(NOT EXISTS "${map_storage_root}/extern/json/single_include/nlohmann/json.hpp")
    message(FATAL_ERROR "Missing pinned extern/json/single_include/nlohmann/json.hpp")
endif()
if(NOT EXISTS "${map_storage_root}/extern/googletest/CMakeLists.txt")
    message(FATAL_ERROR "Missing pinned extern/googletest/CMakeLists.txt")
endif()
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory("${map_storage_root}/extern/googletest" "${CMAKE_CURRENT_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)

add_library(map_storage_codec STATIC codec.cpp file-storage.cpp)
target_compile_features(map_storage_codec PUBLIC cxx_std_23)
target_include_directories(map_storage_codec PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(map_storage_codec SYSTEM PRIVATE "${map_storage_root}/extern/json/single_include")
set_target_properties(map_storage_codec PROPERTIES CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(map_storage_codec PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(map_storage_cli cli.cpp)
target_link_libraries(map_storage_cli PRIVATE map_storage_codec)
set_target_properties(map_storage_cli PROPERTIES CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(map_storage_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(map_storage_tests codec-tests.cpp file-storage-tests.cpp cli-diagnostics-tests.cpp)
target_link_libraries(map_storage_tests PRIVATE map_storage_codec GTest::gtest_main)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(map_storage_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
enable_testing()
include(GoogleTest)
gtest_discover_tests(map_storage_tests TEST_PREFIX "map_storage." PROPERTIES LABELS "map_storage" DISCOVERY_MODE PRE_TEST)

# Test tooling only; the CLI itself needs only the codec/adapter and C++ runtime.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    foreach(map_storage_case IN ITEMS round_trip comparison malformed truncated format_version missing profiles
                                     arguments help native_path failed_write_retry output_failure)
        add_test(NAME "map_storage.cli.${map_storage_case}"
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/cli-checks.py"
                --cli "$<TARGET_FILE:map_storage_cli>" --case "${map_storage_case}"
                --work-root "${CMAKE_CURRENT_BINARY_DIR}/cli-checks")
        set_tests_properties("map_storage.cli.${map_storage_case}" PROPERTIES LABELS "map_storage")
    endforeach()
endif()
