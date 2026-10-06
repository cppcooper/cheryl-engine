#pragma once

#include "ui-status.h"

#include <core/game-framework/tick-context.h>
#include <core/rendering/render-frame.h>

#include <filesystem>
#include <memory>
#include <string_view>

namespace CE::Engine {
    class EngineContext;
}

// Independent application presentation with native RML/RCSS documents.
class DemoRmlUi final {
    struct State;
    std::unique_ptr<State> state_;

public:
    DemoRmlUi(CE::Engine::EngineContext& engine, const std::filesystem::path& asset_root, std::filesystem::path font);
    ~DemoRmlUi();
    DemoRmlUi(const DemoRmlUi&) = delete;
    DemoRmlUi& operator=(const DemoRmlUi&) = delete;
    bool update(const CE::GFramework::TickContext& tick, const DemoUiStatus& status);
    void write(CE::RenderAPIs::RenderFrameWriter& frame) const;
    [[nodiscard]] std::string_view error() const;
};
