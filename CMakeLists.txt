cmake_minimum_required(VERSION 3.28)
set(CMAKE_CXX_STANDARD 23)
project(cheryl)

option(WARN "Enable compiler warnings [-Wall]" OFF)
option(CHERYL_SANDBOX_BUILD "Build without X11 and Gainput for sandbox environments" OFF)
option(CHERYL_BUILD_TESTS "Build Cheryl's tests and acceptance tools" ON)
option(CHERYL_BUILD_DEMO "Build the windowed demonstration application" ON)
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

# Public headers contain logging templates; every consumer must use the same policy.
set(CHERYL_LOG_PROFILE "auto" CACHE STRING "Logging defaults: auto, developer, support, or release")
set_property(CACHE CHERYL_LOG_PROFILE PROPERTY STRINGS auto developer support release)
set(CHERYL_LOG_COMPILED_MASK "" CACHE STRING "Optional severity-bit override (0x3f trace, 0x1f debug, 0x0f info, 0x07 warn, 0 off)")
add_library(cheryl_logging_config INTERFACE)
if(CHERYL_LOG_PROFILE STREQUAL "auto")
    target_compile_definitions(cheryl_logging_config INTERFACE
        CHERYL_LOG_PROFILE=$<IF:$<CONFIG:Debug>,0,$<IF:$<CONFIG:RelWithDebInfo>,1,2>>)
elseif(CHERYL_LOG_PROFILE STREQUAL "developer")
    target_compile_definitions(cheryl_logging_config INTERFACE CHERYL_LOG_PROFILE=0)
elseif(CHERYL_LOG_PROFILE STREQUAL "support")
    target_compile_definitions(cheryl_logging_config INTERFACE CHERYL_LOG_PROFILE=1)
elseif(CHERYL_LOG_PROFILE STREQUAL "release")
    target_compile_definitions(cheryl_logging_config INTERFACE CHERYL_LOG_PROFILE=2)
else()
    message(FATAL_ERROR "Unknown CHERYL_LOG_PROFILE: ${CHERYL_LOG_PROFILE}")
endif()
if(NOT CHERYL_LOG_COMPILED_MASK STREQUAL "")
    if(NOT CHERYL_LOG_COMPILED_MASK MATCHES "^(0|[1-9][0-9]*|0[xX][0-9a-fA-F]+)$")
        message(FATAL_ERROR "CHERYL_LOG_COMPILED_MASK must be a decimal or hexadecimal integer")
    endif()
    math(EXPR cheryl_logging_mask_value "${CHERYL_LOG_COMPILED_MASK}")
    if(cheryl_logging_mask_value LESS 0 OR cheryl_logging_mask_value GREATER 63)
        message(FATAL_ERROR "CHERYL_LOG_COMPILED_MASK contains unknown severity bits")
    endif()
    target_compile_definitions(cheryl_logging_config INTERFACE CHERYL_LOG_COMPILED_MASK=${CHERYL_LOG_COMPILED_MASK})
endif()

# Keep GLFW's null platform available while avoiding native display dependencies.
if(CHERYL_SANDBOX_BUILD AND UNIX AND NOT APPLE)
    set(GLFW_BUILD_X11 OFF)
    set(GLFW_BUILD_WAYLAND OFF)
endif()

# source files
if(NOT CHERYL_SANDBOX_BUILD)
    if(UNIX AND NOT APPLE)
        find_package(X11 REQUIRED)
    endif()
endif()
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false src/*)
# This object installs process signal handlers only when an application opts in.
list(FILTER LIB_SOURCES EXCLUDE REGEX "/src/main\\.cpp$")
file(GLOB_RECURSE GTEST_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false tests/executables/gtest/*)
if(CHERYL_SANDBOX_BUILD)
    list(FILTER LIB_SOURCES EXCLUDE REGEX "/(glfw-bindings|input-mapper|input-system)\\.cpp$")
endif()
message("Library sources: ${LIB_SOURCES}")
message("Common test sources: ${TEST_SOURCES}")

# dependencies
find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
set(BACKWARD_SHARED STATIC)
set(SPDLOG_USE_STD_FORMAT ON)
set(GLM_ENABLE_EXPERIMENTAL ON)

if(CHERYL_BUILD_TESTS)
    add_subdirectory(extern/googletest EXCLUDE_FROM_ALL)
endif()
add_subdirectory(extern/ctti EXCLUDE_FROM_ALL)
add_subdirectory(extern/backward-cpp EXCLUDE_FROM_ALL)
add_subdirectory(extern/spdlog EXCLUDE_FROM_ALL)
if(NOT CHERYL_SANDBOX_BUILD)
    add_subdirectory(extern/gainput EXCLUDE_FROM_ALL)
endif()
add_subdirectory(extern/glad/cmake)
add_subdirectory(extern/glm EXCLUDE_FROM_ALL)
add_subdirectory(extern/glfw EXCLUDE_FROM_ALL)

# Use the pinned GLAD specification instead of downloading a newer one during compilation.
glad_add_library(gl46 STATIC EXCLUDE_FROM_ALL REPRODUCIBLE API gl:core=4.6 EXTENSIONS GL_EXT_texture_filter_anisotropic GL_KHR_debug )

message("-- Build directory: ${CMAKE_CURRENT_BINARY_DIR}")

# Disable the fallthrough warning for GCC/Clang
if(NOT CHERYL_SANDBOX_BUILD AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gainputstatic PRIVATE -Wno-implicit-fallthrough -Wno-unused-parameter)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wno-deprecated-declarations)
endif()

# library
add_library(${LIB_TARGET} STATIC ${LIB_SOURCES})
add_library(Cheryl::Engine ALIAS ${LIB_TARGET})
target_compile_features(${LIB_TARGET} PUBLIC cxx_std_23)
target_include_directories(${LIB_TARGET} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cheryl>"
    # Preserve the existing public glm.hpp spelling alongside glm/glm.hpp.
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/extern/glm/glm>")
target_include_directories(${LIB_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/stb"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/json/single_include")
target_link_libraries(${LIB_TARGET}
    PUBLIC Threads::Threads cheryl_logging_config ctti spdlog::spdlog glm::glm-header-only Backward::Interface gl46
    PRIVATE glfw OpenGL::GL)
if(NOT CHERYL_SANDBOX_BUILD)
    # Gainput's target does not publish its public include directory. Its types
    # appear in the native InputSystem/InputMapper headers, so this is public.
    target_include_directories(${LIB_TARGET} SYSTEM PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/extern/gainput/lib/include>")
    target_link_libraries(${LIB_TARGET} PUBLIC gainputstatic)
    if(UNIX AND NOT APPLE)
        target_link_libraries(${LIB_TARGET} PRIVATE X11::X11)
    endif()
endif()
if(CHERYL_SANDBOX_BUILD)
    target_compile_definitions(${LIB_TARGET} PUBLIC CHERYL_SANDBOX_BUILD=1)
endif()
add_dependencies(${LIB_TARGET} gl46)

# Header-only stack capture uses Backward::Interface. Backward's Object/Backward
# targets install their own process signal handler and are not engine dependencies.
add_library(cheryl_signal_handlers OBJECT src/main.cpp)
add_library(Cheryl::SignalHandlers ALIAS cheryl_signal_handlers)
target_compile_features(cheryl_signal_handlers PUBLIC cxx_std_23)
target_link_libraries(cheryl_signal_handlers PUBLIC Backward::Interface)

# testing
if(CHERYL_BUILD_TESTS)
    include(GoogleTest)
    enable_testing()
    add_executable(${TEST_TARGET} tests/executables/gtest.cpp ${GTEST_SOURCES})
    target_link_libraries(${TEST_TARGET} PRIVATE Cheryl::Engine gtest glfw)
    TARGET_INCLUDE_DIRECTORIES(${TEST_TARGET} PRIVATE tests/include src)
    target_compile_definitions(${TEST_TARGET} PRIVATE CHERYL_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND GLFW_BUILD_X11 AND NOT CHERYL_SANDBOX_BUILD)
        set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tests/executables/gtest/backends/native-opengl.cpp"
            APPEND PROPERTY COMPILE_DEFINITIONS CHERYL_NATIVE_X11_TESTS=1)
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # backward-cpp's fallback resolver reads exported symbols in Release builds.
        target_link_options(${TEST_TARGET} PRIVATE -rdynamic)
    endif()
    if(UNIX AND NOT APPLE AND NOT CHERYL_SANDBOX_BUILD)
        target_link_libraries(${TEST_TARGET} PRIVATE X11::X11)
    endif()
    gtest_discover_tests(${TEST_TARGET})

    # The profile matrix compiles only the logging cases against the real library.
    add_executable(cheryl-logging-tests
        tests/executables/gtest.cpp
        tests/executables/gtest/logging.cpp
        tests/executables/gtest/core/logging-policy.cpp)
    target_link_libraries(cheryl-logging-tests PRIVATE Cheryl::Engine gtest)

    # Isolated timeout/fault/static-teardown cases are driven by tests/acceptance/logging.py.
    add_executable(cheryl-logging-acceptance tests/executables/logging-acceptance.cpp)
    target_link_libraries(cheryl-logging-acceptance PRIVATE Cheryl::Engine)

    add_executable(backward-cpp tests/executables/backward.cpp)
    target_link_libraries(backward-cpp PUBLIC Backward::Backward)

    add_executable(cpp-testing tests/executables/cpp-testing.cpp)
endif()

if(CHERYL_BUILD_DEMO AND NOT CHERYL_SANDBOX_BUILD)
    add_executable(demo tests/executables/demo.cpp)
    target_compile_definitions(demo PRIVATE CHERYL_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(demo PRIVATE Cheryl::Engine Cheryl::SignalHandlers)
endif()
