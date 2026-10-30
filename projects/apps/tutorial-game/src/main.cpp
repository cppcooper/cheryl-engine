#include "game.h"

#include <cheryl/application/startup.h>

#include <exception>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    try {
        CE::Application::ApplicationOptions options;
        options.width = 960;
        options.height = 540;
        options.title = "Tutorial: arrows move; F2 joins J/L player; F3 leaves; A/D pan; Space scores; Esc quits";
        auto startup = CE::Application::make_startup("Cheryl tutorial game", options);
        auto result = startup.initialize(argc, argv);
        if (!result.should_start())
            return result.exit_code;
        Tutorial::Game game(*result.context, std::filesystem::path(CHERYL_TUTORIAL_GAME_ASSETS));
        auto runtime = result.make_runtime(game);
        runtime.run();
        return result.exit_code;
    } catch (const std::exception& error) {
        std::cerr << "Tutorial game: " << error.what() << '\n';
        return 1;
    }
}
