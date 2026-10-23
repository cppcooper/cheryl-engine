cmake_minimum_required(VERSION 3.28)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cheryl_tutorial_game LANGUAGES CXX)
endif()

if(NOT TARGET Cheryl::Startup)
    find_package(Cheryl 4 CONFIG REQUIRED)
endif()
foreach(tutorial_dependency IN ITEMS Cheryl::Startup Cheryl::NativeGLFW Cheryl::OpenGL Cheryl::UI::TGUI)
    if(NOT TARGET ${tutorial_dependency})
        message(FATAL_ERROR "tutorial-game requires ${tutorial_dependency}")
    endif()
endforeach()
get_target_property(tutorial_native_input Cheryl::NativeGLFW CHERYL_NATIVE_INPUT)
if(NOT tutorial_native_input)
    message(FATAL_ERROR "tutorial-game requires Native GLFW with native input enabled")
endif()
get_target_property(tutorial_startup_opengl Cheryl::Startup CHERYL_STARTUP_OPENGL)
if(NOT tutorial_startup_opengl)
    message(FATAL_ERROR "tutorial-game requires Startup's configured OpenGL integration")
endif()

add_executable(tutorial-game
    src/main.cpp
    src/game.cpp
    src/game.h
    src/settings.cpp
    src/settings.h
    src/score-input.cpp
    src/score-input.h
    src/character-command.h
    src/character.h
    src/player-controller.h
    src/patrol-controller.h
    src/ui-controller.h
    src/hud.h)
target_compile_features(tutorial-game PRIVATE cxx_std_23)
set(tutorial_json_headers "${CMAKE_CURRENT_SOURCE_DIR}/../../../extern/json/single_include")
target_include_directories(tutorial-game PRIVATE "${tutorial_json_headers}")
target_compile_definitions(tutorial-game PRIVATE
    CHERYL_TUTORIAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_link_libraries(tutorial-game PRIVATE
    Cheryl::Startup Cheryl::NativeGLFW Cheryl::OpenGL Cheryl::UI::TGUI)
if(DEFINED CHERYL_BUILD_ROOT)
    set_target_properties(tutorial-game PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
endif()

target_sources(tutorial-game PRIVATE
    assets/graphics/graphics-manifests.json
    assets/graphics/definitions/sprites.json
    assets/graphics/textures/actor.png)

if(CHERYL_BUILD_TESTS AND TARGET "${TARGET_TEST_ALL_TESTS}")
    set(CHERYL_TEST_AGGREGATE ${TARGET_TEST_ALL_TESTS})
    cheryl_add_test_suite(tests-tutorial-game
        SOURCES tests/gameplay.cpp tests/settings.cpp tests/score-input.cpp src/settings.cpp src/score-input.cpp
        PRIVATE_INCLUDES "${tutorial_json_headers}"
        LIBRARIES Cheryl::Engine)
endif()
