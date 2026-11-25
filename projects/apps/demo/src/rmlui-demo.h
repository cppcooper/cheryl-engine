#pragma once

#include "ui-status.h"

#include <core/game-framework/tick-context.h>
#include <core/rendering/render-frame.h>

#include <filesystem>
#include <memory>
#include <string_view>

namespace CE::Runtime {
    class Context;
}
namespace DemoConsole {
    class Application;
}
class DemoTguiControls;

// Independent application presentation with native RML/RCSS documents.
class DemoRmlUi final {
    struct RmlUiState;
    std::unique_ptr<RmlUiState> rmlui_state_;

public:
    DemoRmlUi(CE::Runtime::Context& engine, const std::filesystem::path& asset_root, DemoConsole::Application* console = nullptr);
    ~DemoRmlUi();
    DemoRmlUi(const DemoRmlUi&) = delete;
    DemoRmlUi& operator=(const DemoRmlUi&) = delete;
    void init_simulation();
    void stop_console() noexcept;
    bool process_input(const CE::Game::TickContext& tick, DemoTguiControls* tgui = nullptr);
    bool update(const CE::Game::TickContext& tick, const DemoUiStatus& status);
    void write(CE::Rendering::FrameWriter& frame) const;
    [[nodiscard]] std::string_view error() const;
};
