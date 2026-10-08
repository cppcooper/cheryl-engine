#pragma once

#include <text/font-collection.h>
#include <text/layout.h>

#include <filesystem>

namespace CE::Engine {
    class Startup;
}

struct DemoOptions {
    std::filesystem::path asset_root;
    bool load_all_assets = false;
    bool unicode_preview = false;
    CE::Text::FontSelection font_selection;
    CE::Text::LayoutOptions text_options;
    unsigned int max_updates = 0;

    DemoOptions();
    // Keep this object at a stable address until startup.initialize() returns.
    void register_with(CE::Engine::Startup& startup);
};
