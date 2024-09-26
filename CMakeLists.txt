cmake_minimum_required(VERSION 3.19)
set(CMAKE_CXX_STANDARD 23)
project(cheryl)

option(WARN "Enable compiler warnings [-Wall]" OFF)
if(WARN)
    set(GCC_COVERAGE_COMPILE_FLAGS "-Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}" )
endif(WARN)
if (NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("Debug build detected: enabling UB Sanitizer.")
    add_compile_options("-fsanitize=address,undefined")
    add_link_options("-fsanitize=address,undefined")
endif()

# target names
set(LIB_TARGET cherylGL)
set(TEST_TARGET all-tests)

# source files
find_package(X11)
file(GLOB_RECURSE LIB_SOURCES LIST_DIRECTORIES false src/* ${CMAKE_CURRENT_BINARY_DIR}/_deps/glad-src/src/*)
file(GLOB_RECURSE GTEST_SOURCES LIST_DIRECTORIES false tests/executables/gtest/*)
message("Library sources: ${LIB_SOURCES}")
message("Common test sources: ${TEST_SOURCES}")

# dependencies
set(BACKWARD_SHARED STATIC)
set(SPDLOG_USE_STD_FORMAT ON)
set(GLM_ENABLE_EXPERIMENTAL ON)

add_subdirectory(extern/googletest EXCLUDE_FROM_ALL)
add_subdirectory(extern/ctti EXCLUDE_FROM_ALL)
add_subdirectory(extern/backward-cpp EXCLUDE_FROM_ALL)
add_subdirectory(extern/spdlog EXCLUDE_FROM_ALL)
add_subdirectory(extern/gainput EXCLUDE_FROM_ALL)
add_subdirectory(extern/glad/cmake)
add_subdirectory(extern/glm EXCLUDE_FROM_ALL)
add_subdirectory(extern/glfw EXCLUDE_FROM_ALL)

glad_add_library(gl46 STATIC EXCLUDE_FROM_ALL API gl:core=4.6 EXTENSIONS GL_EXT_texture_filter_anisotropic )

# includes
include_directories(include)
include_directories(include/cheryl)
include_directories(extern/googletest/googletest/include)
include_directories(extern/ctti/include)
include_directories(extern/backward-cpp)
include_directories(extern/spdlog/include)
include_directories(extern/gainput/lib/include)
include_directories(extern/stb)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/gladsources/gl46/include)
include_directories(extern/glm/glm)
include_directories(extern/glfw/include)
include_directories(extern/json/single_include)

message("-- Build directory: ${CMAKE_CURRENT_BINARY_DIR}")

# Disable the fallthrough warning for GCC/Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gainputstatic PRIVATE -Wno-implicit-fallthrough -Wno-unused-parameter)
    add_compile_options(-Wno-deprecated-declarations)
endif()

# library
add_library(${LIB_TARGET} STATIC ${LIB_SOURCES})
#target_link_libraries(spdlog Backward::Object gainputstatic ${X11_LIBRARIES} glfw gl46 GL)
add_dependencies(${LIB_TARGET} gl46)

# testing
include(GoogleTest)
enable_testing()
add_executable(${TEST_TARGET} tests/executables/gtest.cpp ${GTEST_SOURCES} ${LIB_SOURCES})
TARGET_INCLUDE_DIRECTORIES(${TEST_TARGET} PRIVATE tests/include)
target_link_libraries(${TEST_TARGET} PRIVATE gtest spdlog Backward::Object gainputstatic ${X11_LIBRARIES} glfw gl46 GL)
gtest_discover_tests(${TEST_TARGET})

add_executable(backward-cpp tests/executables/backward.cpp)
target_link_libraries(backward-cpp PUBLIC Backward::Backward)

add_executable(cpp-testing tests/executables/cpp-testing.cpp)
