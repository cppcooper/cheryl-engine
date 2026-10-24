#pragma once

#include "generated-document.h"

#include <assets/submission/tilemap2d.h>
#include <core/controls/input-record.h>
#include <core/display/framebuffer-size.h>
#include <core/display/viewport.h>
#include <core/rendering/render-frame.h>
#include <maps/terrain-map.h>

#include <array>
#include <memory>
#include <optional>
#include <span>

// Private application state, shared with synthetic consumers. Platform constructs
// it before simulation starts; update/preparation use it serially on simulation.
// No input/window/resource provider or borrowed tick/frame view is retained.
class DemoTilemapEditor final {
public:
    enum class Mode { Literal, Terrain };

    // Diagnostic colors only: authored N/E/S/W masks select original targets,
    // mapped explicitly to the four retained logical IDs. Zero entries are holes.
    struct TerrainPalette {
        CE::Assets::Tilesets::BitmaskRule rule;
        std::vector<CE::Tilemaps::TileId> target_to_id;
    };

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

    WorldProof::KeyTable document_keys_;
    std::array<std::unique_ptr<WorldProof::LiteralOwner>, 2> documents_;
    std::filesystem::path document_dir_;
    std::array<std::unique_ptr<WorldProof::TerrainDocument>, 2> terrain_;
    std::array<std::unique_ptr<WorldProof::GeneratedDocument>, 2> generated_;
    CE::Tilemaps::TerrainBatchLimits terrain_limits_;
    WorldProof::HistoryLimits history_limits_;
    std::array<std::array<WorldProof::DocumentHistory, 2>, 2> histories_;
    Mode mode_ = Mode::Literal;
    CE::Tilemaps::TerrainLabel terrain_brush_{1};
    Sizes sizes_;
    CE::Tilemaps::LocalPoint pan_{0, 0};
    double zoom_ = 1;
    std::size_t selected_map_ = 0;
    CE::Tilemaps::LayerIndex selected_layer_{0};
    CE::Tilemaps::TileId brush_{7};
    std::optional<Geometry> geometry_;

    static std::optional<Geometry> geometry(Sizes sizes, CE::Tilemaps::LocalPoint pan, double zoom);
    void document_action(bool save);
    [[nodiscard]] WorldProof::DocumentHistory& selected_history();
    void literal_edit(CE::Tilemaps::Point point, CE::Tilemaps::TileId id);
    void terrain_edit(std::span<const CE::Tilemaps::TerrainEdit> edits);
    void history_action(bool undo);

public:
    explicit DemoTilemapEditor(std::filesystem::path document_dir = {}, WorldProof::HistoryLimits history_limits = {});
    explicit DemoTilemapEditor(
        TerrainPalette palette,
        CE::Tilemaps::TerrainBatchLimits limits = {32, 160, 1024},
        std::filesystem::path document_dir = {},
        WorldProof::HistoryLimits history_limits = {}
    );
    DemoTilemapEditor(const DemoTilemapEditor&) = delete;
    DemoTilemapEditor& operator=(const DemoTilemapEditor&) = delete;
    DemoTilemapEditor(DemoTilemapEditor&&) = delete;
    DemoTilemapEditor& operator=(DemoTilemapEditor&&) = delete;
    // Explicit semantic-mode epoch/preserve policy; no implicit mode/map selection.
    void
    install_generated(const WorldProof::CaveRecipe& recipe, WorldProof::GeneratedReplace policy, WorldProof::GeneratedLimits limits = {});
    void regenerate_selected();
    [[nodiscard]] std::optional<WorldProof::GeneratedDocument> generated_state(std::size_t index) const;
    [[nodiscard]] static TerrainPalette diagnostic_terrain();
    // Records are already in publication order. Use this tick's copied sizes;
    // they are not per-event historical resize samples. Invalid sizes suspend
    // edits/preparation until a valid sample, preserving maps/view/selection.
    // Terrain paint/erase runs flush atomically at keyboard Press, valid wheel,
    // middle sample and call end. Refusals propagate; earlier runs stay applied.
    // Release/repeat, final pointer axes and misses neither edit nor invent history.
    void consume(std::span<const CE::Input::Events::Record> records, Sizes sizes);
    // Checked whole-view replacement; invalid candidates preserve the old view.
    bool set_view(CE::Tilemaps::LocalPoint pan, double zoom);
    [[nodiscard]] std::optional<CE::Tilemaps::Point> pick(CE::Input::Events::Pointer pointer) const;
    void write(
        CE::Rendering::FrameWriter& frame,
        const CE::Rendering::Submission::TilemapBindings& bindings,
        const CE::Rendering::DrawStyle2D& images
    ) const;

    // Content edits, undo/redo and LOAD replace owners, invalidating borrowed
    // document/map/terrain views. No-ops preserve them; owned values/packets stay
    // independent. Literal storage remains available in either mode.
    [[nodiscard]] const CE::Tilemaps::Map& map(std::size_t index) const { return documents_.at(index)->map(); }
    [[nodiscard]] const WorldProof::LiteralOwner& document(std::size_t index) const { return *documents_.at(index); }
    [[nodiscard]] const CE::Tilemaps::TerrainMap& terrain(std::size_t index, CE::Tilemaps::LayerIndex layer) const {
        return terrain_.at(index)->terrain(layer);
    }
    [[nodiscard]] const WorldProof::TerrainDocument& terrain_document(std::size_t index) const { return *terrain_.at(index); }
    [[nodiscard]] WorldProof::HistoryState history(std::size_t index, Mode mode) const {
        return histories_.at(index).at(mode == Mode::Literal ? 0 : 1).state();
    }
    [[nodiscard]] Mode mode() const { return mode_; }
    [[nodiscard]] CE::Tilemaps::TerrainLabel terrain_brush() const { return terrain_brush_; }
    [[nodiscard]] std::size_t selected_map() const { return selected_map_; }
    [[nodiscard]] CE::Tilemaps::LayerIndex selected_layer() const { return selected_layer_; }
    [[nodiscard]] CE::Tilemaps::TileId brush() const { return brush_; }
    [[nodiscard]] CE::Tilemaps::LocalPoint pan() const { return pan_; }
    [[nodiscard]] double zoom() const { return zoom_; }
};
