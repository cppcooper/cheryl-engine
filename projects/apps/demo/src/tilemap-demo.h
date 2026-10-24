#pragma once

#include "tilemap-editor.h"

#include <filesystem>
#include <utility>

namespace CE::Rendering::Resources {
    struct Provider;
}

// Load/capture on platform, edit/prepare serially on simulation, destroy on
// platform after the runtime has finished its frames. Assets remain bind-once.
class DemoTilemaps final {
    DemoTilemapEditor editor_;
    CE::Rendering::Submission::TilemapBindings bindings_;

public:
    explicit DemoTilemaps(std::filesystem::path document_dir = {})
    : editor_(std::move(document_dir)) {}
    void load(const std::filesystem::path& root, CE::Rendering::Resources::Provider& provider);
    void
    install_generated(const WorldProof::CaveRecipe& recipe, WorldProof::GeneratedReplace policy, WorldProof::GeneratedLimits limits = {}) {
        editor_.install_generated(recipe, policy, limits);
    }
    void update(std::span<const CE::Input::Events::Record> records, DemoTilemapEditor::Sizes sizes);
    void write(CE::Rendering::FrameWriter& frame, const CE::Rendering::DrawStyle2D& images) const;
};
