cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/CherylModules.cmake")
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cheryl_debug_terminal_linux_module VERSION ${VERSION_PROJECT_MODULE_DEBUG_TERMINAL_LINUX} LANGUAGES C CXX)
endif()
if(TARGET Cheryl::DebugTerminal::Linux)
    return()
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "The Linux Debug terminal module requires Linux with GNU or Clang")
endif()
cheryl_configure_module_options()
cheryl_prepare_module()

cheryl_configure_debug_terminal_options()

add_library(${TARGET_LIB_MODULE_DEBUG_TERMINAL_LINUX} INTERFACE)
add_library(Cheryl::DebugTerminal::Linux ALIAS ${TARGET_LIB_MODULE_DEBUG_TERMINAL_LINUX})
cheryl_register_public_component(${TARGET_LIB_MODULE_DEBUG_TERMINAL_LINUX}
    FEATURE CHERYL_HAS_DEBUG_TERMINAL_LINUX BUILD_ONLY)
target_include_directories(${TARGET_LIB_MODULE_DEBUG_TERMINAL_LINUX} INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cheryl>")
target_compile_definitions(${TARGET_LIB_MODULE_DEBUG_TERMINAL_LINUX} INTERFACE
    "CHERYL_DEBUG_TERMINAL_AVAILABLE=$<BOOL:${CHERYL_BUILD_DEBUG_TERMINAL_LINUX}>")

if(CHERYL_BUILD_DEBUG_TERMINAL_LINUX)
    if(NOT TARGET Cheryl::DebugTerminal::Startup)
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/private/startup"
            "${CMAKE_CURRENT_BINARY_DIR}/startup" EXCLUDE_FROM_ALL)
    endif()
    if(NOT TARGET Threads::Threads)
        find_package(Threads REQUIRED)
    endif()

    add_executable(cheryl-debug-terminal-viewer EXCLUDE_FROM_ALL src/viewer.cpp)
    target_compile_features(cheryl-debug-terminal-viewer PRIVATE cxx_std_23)
    set_target_properties(cheryl-debug-terminal-viewer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")

    add_library(module_debug_terminal_linux_impl STATIC EXCLUDE_FROM_ALL src/main-wrap.cpp src/terminal.cpp)
    set_target_properties(module_debug_terminal_linux_impl PROPERTIES
        VERSION "${VERSION_PROJECT_MODULE_DEBUG_TERMINAL_LINUX}"
        OUTPUT_NAME "${OUTPUT_LIB_MODULE_DEBUG_TERMINAL_LINUX}"
        ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
    target_link_libraries(module_debug_terminal_linux_impl
        PUBLIC Cheryl::DebugTerminal::Startup
        PRIVATE Threads::Threads)
    target_compile_definitions(module_debug_terminal_linux_impl PRIVATE
        "CHERYL_TERMINAL_VIEWER=\"$<TARGET_FILE:cheryl-debug-terminal-viewer>\""
        "CHERYL_TERMINAL_EMULATOR=\"${CHERYL_DEBUG_TERMINAL_EMULATOR}\"")
    target_link_options(module_debug_terminal_linux_impl INTERFACE "LINKER:--wrap=main")
    add_dependencies(module_debug_terminal_linux_impl cheryl-debug-terminal-viewer)

    target_link_libraries(${TARGET_LIB_MODULE_DEBUG_TERMINAL_LINUX} INTERFACE
        module_debug_terminal_linux_impl)
    include("${CMAKE_CURRENT_SOURCE_DIR}/tests/support/cmake/GoogleTestOutput.cmake")
    cheryl_route_test_reports("1")
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cheryl_select_public_component(${TARGET_LIB_MODULE_DEBUG_TERMINAL_LINUX})
endif()

if(CHERYL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/acceptance)
endif()
