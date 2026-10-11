set(CHERYL_DEBUG_TERMINAL "AUTO" CACHE STRING "Native output terminal inclusion: AUTO (Debug only), ON, or OFF")
set_property(CACHE CHERYL_DEBUG_TERMINAL PROPERTY STRINGS AUTO ON OFF)
set(CHERYL_DEBUG_TERMINAL_EMULATOR "auto" CACHE STRING "Linux native terminal emulator: auto, konsole, or xterm")
set_property(CACHE CHERYL_DEBUG_TERMINAL_EMULATOR PROPERTY STRINGS auto konsole xterm)

if(CHERYL_DEBUG_TERMINAL STREQUAL "AUTO")
    set(cheryl_terminal_configuration "$<CONFIG:Debug>")
elseif(CHERYL_DEBUG_TERMINAL STREQUAL "ON")
    set(cheryl_terminal_configuration "1")
elseif(CHERYL_DEBUG_TERMINAL STREQUAL "OFF")
    set(cheryl_terminal_configuration "0")
else()
    message(FATAL_ERROR "Unknown CHERYL_DEBUG_TERMINAL: ${CHERYL_DEBUG_TERMINAL}")
endif()
if(NOT CHERYL_DEBUG_TERMINAL_EMULATOR MATCHES "^(auto|konsole|xterm)$")
    message(FATAL_ERROR "Unknown CHERYL_DEBUG_TERMINAL_EMULATOR: ${CHERYL_DEBUG_TERMINAL_EMULATOR}")
endif()

set(cheryl_terminal_supported OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(cheryl_terminal_supported ON)
elseif(CHERYL_DEBUG_TERMINAL STREQUAL "ON")
    message(FATAL_ERROR "CHERYL_DEBUG_TERMINAL=ON currently requires Linux with GNU or Clang")
endif()
set(cheryl_terminal_enabled "$<AND:$<BOOL:${cheryl_terminal_supported}>,$<BOOL:${cheryl_terminal_configuration}>>")
set_property(TARGET ${TARGET_LIB_ENGINE} PROPERTY CHERYL_DEBUG_TERMINAL_ENABLED "${cheryl_terminal_enabled}")
target_compile_definitions(${TARGET_LIB_ENGINE} INTERFACE "CHERYL_DEBUG_TERMINAL_AVAILABLE=$<BOOL:${cheryl_terminal_enabled}>")

if(NOT cheryl_terminal_supported OR CHERYL_DEBUG_TERMINAL STREQUAL "OFF")
    return()
endif()

add_executable(cheryl-debug-terminal-viewer EXCLUDE_FROM_ALL src/viewer.cpp)
target_compile_features(cheryl-debug-terminal-viewer PRIVATE cxx_std_23)
set_target_properties(cheryl-debug-terminal-viewer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")

add_library(cheryl-debug-terminal-bootstrap OBJECT EXCLUDE_FROM_ALL src/main-wrap.cpp src/terminal.cpp)
target_compile_features(cheryl-debug-terminal-bootstrap PRIVATE cxx_std_23)
target_link_libraries(cheryl-debug-terminal-bootstrap PRIVATE Threads::Threads)
target_compile_definitions(cheryl-debug-terminal-bootstrap PRIVATE
    "CHERYL_TERMINAL_AUTOMATIC=$<IF:$<CONFIG:Debug>,1,0>"
    "CHERYL_TERMINAL_VIEWER=\"$<TARGET_FILE:cheryl-debug-terminal-viewer>\""
    "CHERYL_TERMINAL_EMULATOR=\"${CHERYL_DEBUG_TERMINAL_EMULATOR}\"")
add_dependencies(cheryl-debug-terminal-bootstrap cheryl-debug-terminal-viewer)
target_link_libraries(${TARGET_LIB_ENGINE} INTERFACE
    "$<${cheryl_terminal_enabled}:$<TARGET_OBJECTS:cheryl-debug-terminal-bootstrap>>")
target_link_options(${TARGET_LIB_ENGINE} INTERFACE "$<${cheryl_terminal_enabled}:LINKER:--wrap=main>")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GoogleTestOutput.cmake")
cheryl_route_test_reports()
