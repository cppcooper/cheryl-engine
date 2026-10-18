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
    src/player.h
    src/enemy.h
    src/hud.h
    src/controls.cpp
    src/controls.h)
target_compile_features(tutorial-game PRIVATE cxx_std_23)
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
    assets/graphics/shaders/materials.json
    assets/graphics/shaders/first-game.vert
    assets/graphics/shaders/first-game.frag
    assets/graphics/textures/actor.png)
