cmake_minimum_required(VERSION 3.28)
project(CherylNpcBehaviorPrivate LANGUAGES CXX)

if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR "Configure this private scenario as a standalone project")
endif()

get_filename_component(NPC_BEHAVIOR_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
include("${NPC_BEHAVIOR_REPOSITORY_ROOT}/cmake/CherylModules.cmake")
block(SCOPE_FOR VARIABLES)
    set(CHERYL_BUILD_TESTS OFF)
    cheryl_prepare_module()
endblock()

add_library(npc-behavior-private STATIC sentry-controller.cpp scenario.cpp)
target_link_libraries(npc-behavior-private PUBLIC Cheryl::Engine)
target_include_directories(npc-behavior-private PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(npc-behavior-private PRIVATE
    "${NPC_BEHAVIOR_REPOSITORY_ROOT}/projects/engine/tests/include")

add_executable(npc-behavior-tests tests.cpp)
target_link_libraries(npc-behavior-tests PRIVATE npc-behavior-private)
add_executable(npc-behavior-consumer consumer.cpp)
target_link_libraries(npc-behavior-consumer PRIVATE npc-behavior-private)

foreach(target IN ITEMS npc-behavior-private npc-behavior-tests npc-behavior-consumer)
    target_compile_features(${target} PUBLIC cxx_std_23)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # Engine's runtime link requirements do not instrument these callers.
        target_compile_options(${target} PRIVATE
            "$<$<CONFIG:Debug>:-fsanitize=address,undefined>"
            "$<$<CONFIG:Debug>:-fno-sanitize-recover=all>"
            "$<$<CONFIG:Debug>:-fno-omit-frame-pointer>")
        target_link_options(${target} PRIVATE
            "$<$<CONFIG:Debug>:-fsanitize=address,undefined>"
            "$<$<CONFIG:Debug>:-fno-sanitize-recover=all>")
    endif()
endforeach()

enable_testing()
foreach(case IN ITEMS notice_stop notice_boundary range_miss lane_miss no_target resume_patrol
                      long_step watch_direction zero_time invalid_input owned_trace trace_capacity)
    add_test(NAME npc_behavior.${case} COMMAND npc-behavior-tests ${case})
    set_tests_properties(npc_behavior.${case} PROPERTIES TIMEOUT 30)
endforeach()
add_test(NAME npc_behavior.consumer COMMAND npc-behavior-consumer)
set_tests_properties(npc_behavior.consumer PROPERTIES TIMEOUT 30)
