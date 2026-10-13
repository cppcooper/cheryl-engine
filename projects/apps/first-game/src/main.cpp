#include "first-game.h"

#include <cheryl/backends/opengl/startup.h>
#include <CLI/CLI.hpp>

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    try {
        // [startup]
        std::string asset_root = CHERYL_FIRST_GAME_ASSETS;
        auto startup = CE::Engine::make_glfw_opengl_startup(
            "Cheryl first game", {.width = 640, .height = 360, .title = "First game: arrows move, Space changes cell, Escape quits"}
        );
        startup.add_application_options([&asset_root](CLI::App& options) {
            options.add_option("--assets", asset_root, "Game asset directory")->default_str(asset_root);
        });
        auto result = startup.initialize(argc, argv);
        if (!result.should_start())
            return result.exit_code;

        FirstGame::Game game(*result.engine, std::filesystem::path(asset_root));
        auto runtime = result.make_runtime(game);
        runtime.run();
        return result.exit_code;
        // [/startup]
    } catch (const std::exception& error) {
        std::cerr << "First game: " << error.what() << '\n';
        return 1;
    }
}
