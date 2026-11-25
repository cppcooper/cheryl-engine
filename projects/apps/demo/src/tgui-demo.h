#pragma once

#include "ui-status.h"

#include <core/game-framework/tick-context.h>
#include <core/rendering/render-frame.h>

#include <cstdint>
#include <memory>
#include <string_view>

namespace CE::Runtime {
    class Context;
}
class DemoTguiControls;

// Application-owned presentation; Game keeps only neutral status and actions.
class DemoUi final {
    struct TguiState;
    std::unique_ptr<TguiState> tgui_state_;

public:
    // Prepare upload dependencies on platform, without creating toolkit objects.
    explicit DemoUi(CE::Runtime::Context& engine);
    // Runtime calls deinit_platform after joining simulation; all widget refs die first.
    ~DemoUi();
    DemoUi(const DemoUi&) = delete;
    DemoUi& operator=(const DemoUi&) = delete;
    // Create widgets during the game's simulation initialization.
    void init_simulation();
    // Retire widgets on simulation before join; quiesce may repeat after join.
    void deinit_simulation();
    // Returns the panel's camera-reset action, independent of its widget type.
    bool update(const CE::Game::TickContext& tick, const DemoUiStatus& status, bool coordinated = false);
    DemoTguiControls& controls();
    void write(CE::Rendering::FrameWriter& frame) const;
    [[nodiscard]] std::string_view error() const;
};
