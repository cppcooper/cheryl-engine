#pragma once

#include "ui-status.h"

#include <core/game-framework/tick-context.h>
#include <core/rendering/render-frame.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>

namespace CE::Engine {
    class EngineContext;
}

// Application-owned presentation; Game keeps only neutral status and actions.
class DemoUi final {
    struct State;
    std::unique_ptr<State> state_;

public:
    // Build application materials on platform, without creating toolkit objects.
    DemoUi(CE::Engine::EngineContext& engine, const std::filesystem::path& asset_root);
    // Runtime calls deinit after joining simulation; all widget refs die first.
    ~DemoUi();
    DemoUi(const DemoUi&) = delete;
    DemoUi& operator=(const DemoUi&) = delete;
    // First update creates widgets on the actual simulation/UI owner.
    // Returns the panel's camera-reset action, independent of its widget type.
    bool update(const CE::GFramework::TickContext& tick, const DemoUiStatus& status);
    void write(CE::RenderAPIs::RenderFrameWriter& frame) const;
    [[nodiscard]] std::string_view error() const;
};
