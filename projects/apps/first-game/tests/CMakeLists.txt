cmake_minimum_required(VERSION 3.28)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cheryl_first_game_levels LANGUAGES C CXX)
    enable_testing() # The nested engine's enable_testing does not register this outer runner.
endif()

get_filename_component(first_game_repository "${CMAKE_CURRENT_LIST_DIR}/../../../.." REALPATH)
if(NOT TARGET Cheryl::Engine)
    block(SCOPE_FOR VARIABLES)
        set(CHERYL_BUILD_TESTS ON)
        foreach(component IN ITEMS FIRST_GAME DEMO TUTORIAL_GAME STARTUP NATIVE_GLFW NATIVE_SDL3
                OPENGL UI_TGUI UI_RMLUI AUDIO_MINIAUDIO DEBUG_TERMINAL_LINUX WORKERS
                WORKER_TOPOLOGY_HWLOC WORKER_TOPOLOGY_JSON SDK DOCS CONSUMER_TESTS
                ALL_TESTS ACCEPTANCE_TESTS)
            set(CHERYL_BUILD_${component} OFF)
        endforeach()
        set(CHERYL_STARTUP_OPENGL OFF)
        add_subdirectory("${first_game_repository}" cheryl EXCLUDE_FROM_ALL)
    endblock()
endif()
if(NOT TARGET cheryl_test_main OR NOT TARGET cheryl_test_support)
    message(FATAL_ERROR "First Game level tests require Cheryl's source test support (CHERYL_BUILD_TESTS=ON)")
endif()

include("${first_game_repository}/cmake/CherylTests.cmake")
if(NOT DEFINED CHERYL_BUILD_ROOT)
    set(CHERYL_BUILD_ROOT "${CMAKE_CURRENT_BINARY_DIR}")
endif()
add_executable(first-game-level-tests level-transition.cpp ../src/first-game.cpp ../src/level-transition.cpp)
target_compile_features(first-game-level-tests PRIVATE cxx_std_23)
target_include_directories(first-game-level-tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../src")
target_link_libraries(first-game-level-tests PRIVATE Cheryl::Engine cheryl_test_main)
set_target_properties(first-game-level-tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")

# These outer-directory sources do not inherit the nested engine's instrumentation.
if(NOT MSVC)
    target_compile_options(first-game-level-tests PRIVATE "$<$<CONFIG:Debug>:-fsanitize=address,undefined>")
    target_link_options(first-game-level-tests PRIVATE "$<$<CONFIG:Debug>:-fsanitize=address,undefined>")
endif()
cheryl_discover_tests(first-game-level-tests)
