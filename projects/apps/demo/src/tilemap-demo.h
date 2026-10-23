#pragma once

#include <assets/submission/tilemap2d.h>
#include <core/rendering/render-frame.h>

#include <filesystem>

namespace CE::Rendering::Resources {
    struct Provider;
}

// Fixed application fixture: load on platform, read during preparation, destroy
// on platform after the runtime has finished its frames. No editor/input state.
class DemoTilemaps final {
    CE::Tilemaps::Map west_{{3, 2}, 2};
    CE::Tilemaps::Map east_{{3, 2}, 2};
    CE::Rendering::Submission::TilemapBindings bindings_;

public:
    DemoTilemaps();
    void load(const std::filesystem::path& root, CE::Rendering::Resources::Provider& provider);
    void write(
        CE::Rendering::PassWriter& pass,
        const CE::Rendering::DrawStyle2D& images,
        const CE::Rendering::Submission::Context2D& context
    ) const;
};
