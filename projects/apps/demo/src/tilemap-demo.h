#pragma once

#include "tilemap-editor.h"

#include <filesystem>

namespace CE::Rendering::Resources {
    struct Provider;
}

// Load/capture on platform, edit/prepare serially on simulation, destroy on
// platform after the runtime has finished its frames. Assets remain bind-once.
class DemoTilemaps final {
    DemoTilemapEditor editor_;
    CE::Rendering::Submission::TilemapBindings bindings_;

public:
    void load(const std::filesystem::path& root, CE::Rendering::Resources::Provider& provider);
    void update(std::span<const CE::Input::Events::Record> records, DemoTilemapEditor::Sizes sizes) { editor_.consume(records, sizes); }
    void write(CE::Rendering::FrameWriter& frame, const CE::Rendering::DrawStyle2D& images) const;
};
