cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options("-fsanitize=address,undefined")
    add_link_options("-fsanitize=address,undefined")
endif()

# project variables
project(cheryl-engine)

# target names
set(LIBRARY_TARGET cengine)
set(TEST_TARGET all-tests)

# dependencies
file(GLOB_RECURSE SOURCE LIST_DIRECTORIES false src/*)
message("source files: ${SOURCE}")

#add_subdirectory(extern/glew EXCLUDE_FROM_ALL)
add_subdirectory(extern/glfw EXCLUDE_FROM_ALL)
add_subdirectory(extern/glm EXCLUDE_FROM_ALL)
add_subdirectory(extern/spdlog EXCLUDE_FROM_ALL)
add_subdirectory(extern/backward-cpp EXCLUDE_FROM_ALL)
add_subdirectory(extern/googletest EXCLUDE_FROM_ALL)

# include directories
include_directories(include)
include_directories(extern/glm/glm)
include_directories(extern/googletest/googletest/include)
#include_directories(extern/glfw/include/GLFW)
#include_directories(extern/spdlog/include)

# test targets


# library targets
add_library(${LIBRARY_TARGET} SHARED ${SOURCE})
