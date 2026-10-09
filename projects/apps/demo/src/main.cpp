#include <backends/opengl/startup.h>

#include "demo-game.h"
#include "demo-options.h"

#include <iostream>
#include <utility>

int main(const int argc, char** argv) {
    DemoOptions options;
    auto startup = CE::Engine::make_glfw_opengl_startup("Cheryl demo");
    options.register_with(startup);
    auto result = startup.initialize(argc, argv);
    if (!result.should_start())
        return result.exit_code;

    DemoGame game(
        *result.engine,
        options.asset_root,
        options.load_all_assets,
        std::move(options.font_selection),
        std::move(options.text_options),
        options.unicode_preview
    );
    auto game_runtime = result.make_runtime(game);
    if (options.max_updates != 0)
        game.stop_after_updates(options.max_updates, [&game_runtime] { game_runtime.stop(); });
    game_runtime.run();
    if (options.max_updates != 0)
        std::cout << "Completed " << game.completed_updates() << " demo updates\n";
    return result.exit_code;
}
