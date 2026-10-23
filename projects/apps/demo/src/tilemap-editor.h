#pragma once

#include <assets/submission/tilemap2d.h>
#include <core/controls/input-record.h>
#include <core/display/framebuffer-size.h>
#include <core/display/viewport.h>
#include <core/rendering/render-frame.h>

#include <array>
#include <optional>
#include <span>

// Private application state, shared with synthetic consumers. Platform constructs
// it before simulation starts; update/preparation use it serially on simulation.
// No input/window/resource provider or borrowed tick/frame view is retained.
class DemoTilemapEditor final {
public:
    struct Sizes {
        CE::Platform::Dimensions<int> logical{0, 0};
        CE::Platform::FramebufferSize framebuffer{0, 0};
    };

private:
    struct Geometry {
        glm::mat4 projection;
        glm::mat4 view;
        std::array<CE::Rendering::Submission::TilemapView, 2> maps;
    };

    std::array<CE::Tilemaps::Map, 2> maps_{CE::Tilemaps::Map{{3, 2}, 2}, CE::Tilemaps::Map{{3, 2}, 2}};
    Sizes sizes_;
    CE::Tilemaps::LocalPoint pan_{0, 0};
    double zoom_ = 1;
    std::size_t selected_map_ = 0;
    CE::Tilemaps::LayerIndex selected_layer_{0};
    CE::Tilemaps::TileId brush_{7};
    std::optional<Geometry> geometry_;

    static std::optional<Geometry> geometry(Sizes sizes, CE::Tilemaps::LocalPoint pan, double zoom);

public:
    DemoTilemapEditor();
    // Records are already in publication order. Use this tick's copied sizes;
    // they are not per-event historical resize samples. Invalid sizes suspend
    // edits/preparation until a valid sample, preserving maps/view/selection.
    void consume(std::span<const CE::Input::Events::Record> records, Sizes sizes);
    // Checked whole-view replacement; invalid candidates preserve the old view.
    bool set_view(CE::Tilemaps::LocalPoint pan, double zoom);
    [[nodiscard]] std::optional<CE::Tilemaps::Point> pick(CE::Input::Events::Pointer pointer) const;
    void write(
        CE::Rendering::FrameWriter& frame,
        const CE::Rendering::Submission::TilemapBindings& bindings,
        const CE::Rendering::DrawStyle2D& images
    ) const;

    [[nodiscard]] const CE::Tilemaps::Map& map(std::size_t index) const { return maps_.at(index); }
    [[nodiscard]] std::size_t selected_map() const { return selected_map_; }
    [[nodiscard]] CE::Tilemaps::LayerIndex selected_layer() const { return selected_layer_; }
    [[nodiscard]] CE::Tilemaps::TileId brush() const { return brush_; }
    [[nodiscard]] CE::Tilemaps::LocalPoint pan() const { return pan_; }
    [[nodiscard]] double zoom() const { return zoom_; }
};
