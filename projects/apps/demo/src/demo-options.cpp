#include "demo-options.h"

#include <core/engine/startup.h>

#include <CLI/CLI.hpp>

#include <string>

DemoOptions::DemoOptions()
: asset_root(std::filesystem::path(CHERYL_SOURCE_DIR) / "assets") {}

void DemoOptions::register_with(CE::Engine::Startup& startup) {
    startup.add_application_options([this](CLI::App& options) {
        options
            .add_option_function<std::string>(
                "assets", [this](const std::string& path) { asset_root = std::filesystem::path(path); }, "Asset root"
            )
            ->type_name("PATH")
            ->default_str(asset_root.string())
            ->trigger_on_parse();
        options.add_flag("--full-assets", load_all_assets, "Load all available assets")->trigger_on_parse();
        options.add_flag("--unicode-text", unicode_preview, "Show the Unicode text preview")->trigger_on_parse();
        options
            .add_flag_callback(
                "--builtin-font", [this] { font_selection.automatic_system_fonts = false; }, "Disable automatic system font discovery"
            )
            ->disable_flag_override()
            ->trigger_on_parse();
        options
            .add_option_function<std::string>(
                "--font",
                [this](const std::string& path) { font_selection.preferred.push_back(CE::Text::FontFile{std::filesystem::path(path)}); },
                "Preferred font file; repeat to add ordered fallbacks"
            )
            ->trigger_on_parse();
        options
            .add_option_function<std::string>(
                "--font-family",
                [this](const std::string& family) { font_selection.preferred.push_back(CE::Text::SystemFontFamily{family}); },
                "Preferred system font family; repeat to add ordered fallbacks"
            )
            ->trigger_on_parse();
        options
            .add_option_function<std::string>(
                "--text-direction",
                [this](const std::string& direction) {
                    text_options.direction = direction == "auto"  ? CE::Text::ParagraphDirection::Automatic
                                             : direction == "ltr" ? CE::Text::ParagraphDirection::LeftToRight
                                                                  : CE::Text::ParagraphDirection::RightToLeft;
                },
                "Paragraph direction"
            )
            ->check(CLI::IsMember({"auto", "ltr", "rtl"}))
            ->trigger_on_parse();
        options.add_option("--max-updates", max_updates, "Stop after this many demo updates; zero runs until stopped")
            ->check(CLI::NonNegativeNumber)
            ->trigger_on_parse();
    });
}
