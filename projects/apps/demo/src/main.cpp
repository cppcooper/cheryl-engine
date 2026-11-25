#include <cheryl/modules/startup.h>

#include "demo-game.h"
#include "demo-options.h"
#ifdef CHERYL_DEMO_CONSOLE
#include "developer-console-model.h"
#endif

#include <iostream>
#include <memory>
#include <utility>

int main(const int argc, char** argv) {
    DemoOptions options;
    auto startup = CE::Application::make_startup("Cheryl demo");
    options.register_with(startup);
    auto result = startup.initialize(argc, argv);
    if (!result.should_start())
        return result.exit_code;

    DemoConsole::Application* console = nullptr;
#ifdef CHERYL_DEMO_CONSOLE
    DemoConsole::Scenarios::Config console_config;
    console_config.script_root = CHERYL_DEMO_SCRIPT_ROOT;
    console_config.asset_root = CHERYL_DEMO_MAGIC_ROOT;
    console_config.output_root = CHERYL_DEMO_OUTPUT_ROOT;
    console_config.build_id = "cheryl-demo-contact-console";
    auto console_lifetime = std::make_unique<DemoConsole::Application>(std::move(console_config));
    console = console_lifetime.get();
#endif
    {
        DemoGame game(
            *result.context, options.asset_root, options.load_all_assets, std::move(options.font_selection), std::move(options.text_options),
            options.unicode_preview, options.text_runs, console
        );
        auto game_runtime = result.make_runtime(game);
        if (options.max_updates != 0)
            game.stop_after_updates(options.max_updates, [&game_runtime] { game_runtime.stop(); });
        game_runtime.run();
        if (options.max_updates != 0)
            std::cout << "Completed " << game.completed_updates() << " demo updates\n";
    }
#ifdef CHERYL_DEMO_CONSOLE
    console_lifetime->finish();
#endif
    return result.exit_code;
}
