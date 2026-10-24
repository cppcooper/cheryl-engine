#include "tilemap-editor.h"

#include <internals/exceptions.h>
#include <ext/matrix_clip_space.hpp>

#include <cmath>
#include <limits>
#include <utility>

namespace {
    namespace Maps = CE::Tilemaps;
    namespace Submit = CE::Rendering::Submission;
    namespace Events = CE::Input::Events;
    constexpr std::array<Maps::LocalPoint, 2> origins{Maps::LocalPoint{48, 240}, Maps::LocalPoint{328, 208}};
    constexpr Maps::CellSize cells{64, 48};

    WorldProof::KeyTable document_keys() {
        const std::array entries{
            WorldProof::KeyId{"tilemap:brown", {7}}, WorldProof::KeyId{"tilemap:black", {8}}, WorldProof::KeyId{"tilemap:grey", {9}},
            WorldProof::KeyId{"tilemap:red", {10}}
        };
        return WorldProof::KeyTable(entries);
    }

    bool finite(const glm::mat4& matrix) {
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                if (!std::isfinite(matrix[column][row]))
                    return false;
        return true;
    }

    bool representable(const double value) {
        return std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max();
    }
}

DemoTilemapEditor::TerrainPalette DemoTilemapEditor::diagnostic_terrain() {
    namespace Tiles = CE::Assets::Tilesets;
    TerrainPalette result;
    result.rule.type = Tiles::BitmaskType::FourNeighbor;
    result.rule.bit_order = {Tiles::Direction::North, Tiles::Direction::East, Tiles::Direction::South, Tiles::Direction::West};
    for (std::uint32_t mask = 0; mask < 16; ++mask)
        result.rule.cases.emplace(mask, mask % 4);
    result.target_to_id = {{7}, {8}, {9}, {10}};
    return result;
}

DemoTilemapEditor::DemoTilemapEditor(std::filesystem::path document_dir)
: DemoTilemapEditor(diagnostic_terrain(), {32, 6, 256}, std::move(document_dir)) {}

DemoTilemapEditor::DemoTilemapEditor(TerrainPalette palette, const Maps::TerrainBatchLimits limits, std::filesystem::path document_dir)
: document_keys_(document_keys()), document_dir_(std::move(document_dir)), terrain_limits_(limits) {
    for (const auto id : palette.target_to_id)
        if (id.value != 0 && (id.value < 7 || id.value > 10))
            throw CE::Exceptions::invalid_args(CE_HERE, "Private terrain palette supports only logical IDs 7..10 or unmapped zero");
    // Numeric meaning IDs are deliberately unrelated to labels, drawable IDs
    // 7..10 and atlas targets 0..3. Tables/rules are owned after construction.
    const std::array meaning_ids{
        WorldProof::KeyId{"terrain:one", {101}}, WorldProof::KeyId{"terrain:two", {203}}, WorldProof::KeyId{"terrain:three", {307}},
        WorldProof::KeyId{"terrain:four", {409}}
    };
    const WorldProof::KeyTable meaning_keys(meaning_ids);
    const std::array meanings{
        WorldProof::SemanticKey{"terrain:one", {1}}, WorldProof::SemanticKey{"terrain:two", {2}},
        WorldProof::SemanticKey{"terrain:three", {3}}, WorldProof::SemanticKey{"terrain:four", {4}}
    };
    for (std::size_t instance = 0; instance < terrain_.size(); ++instance) {
        const CE::Maps::Snapshot source{
            instance == 0 ? "terrain:west" : "terrain:east",
            3,
            2,
            {{"ground", std::vector<std::optional<std::string>>(6)}, {"detail", std::vector<std::optional<std::string>>(6)}},
            {{"entry", "ground", 0, 0}, {"marker", "detail", static_cast<std::uint32_t>(instance + 1), 0}}
        };
        terrain_[instance] = WorldProof::TerrainDocument::from(source, meaning_keys, meanings, palette.rule, palette.target_to_id);
    }
    constexpr std::array<std::array<std::uint32_t, 6>, 4> ids{
        {{7, 8, 9, 10, 0, 7}, {0, 10, 0, 9, 0, 0}, {9, 7, 0, 8, 10, 9}, {10, 0, 10, 0, 7, 0}}
    };
    for (std::size_t instance = 0; instance < documents_.size(); ++instance) {
        CE::Maps::Snapshot source{
            instance == 0 ? "tilemap:west" : "tilemap:east",
            3,
            2,
            {{"ground", std::vector<std::optional<std::string>>(6)}, {"detail", std::vector<std::optional<std::string>>(6)}},
            {{"entry", "ground", 0, 0}, {"marker", "detail", static_cast<std::uint32_t>(instance + 1), 0}}
        };
        for (std::size_t layer = 0; layer < 2; ++layer)
            for (std::size_t row = 0; row < 2; ++row)
                for (std::size_t column = 0; column < 3; ++column)
                    source.layers[layer].cells[row * 3 + column] = document_keys_.key({ids[instance * 2 + layer][row * 3 + column]});
        documents_[instance] = WorldProof::LiteralOwner::from(source, document_keys_);
    }
}

void DemoTilemapEditor::document_action(const bool save) {
    if (document_dir_.empty())
        throw CE::Exceptions::bad_request(CE_HERE, "Document SAVE/LOAD is disabled; set --tilemap-document-dir PATH");
    std::error_code error;
    if (!std::filesystem::is_directory(document_dir_, error))
        throw CE::Exceptions::bad_request(
            CE_HERE,
            "Document directory is unavailable: " + document_dir_.string() + (error ? ": " + error.message() : " (not a directory)")
        );
    const bool semantic = mode_ == Mode::Terrain;
    const auto path = document_dir_ / (semantic ? (selected_map_ == 0 ? "west-terrain.json" : "east-terrain.json")
                                                : (selected_map_ == 0 ? "west.json" : "east.json"));
    if (save) {
        CE::Maps::save(path, semantic ? terrain_[selected_map_]->snapshot() : documents_[selected_map_]->snapshot());
    } else {
        const auto source = CE::Maps::load(path);
        if (source.width != 3 || source.height != 2 || source.layers.size() != 2)
            throw CE::Exceptions::bad_request(CE_HERE, "Document LOAD requires exactly 3x2 cells and two ordered layers");
        if (semantic) {
            auto candidate = terrain_[selected_map_]->replacement(source);
            terrain_[selected_map_].swap(candidate); // BOTH layers, metadata and rules precede adoption.
        } else {
            if (source.id.starts_with("terrain:"))
                throw WorldProof::Error(WorldProof::Refusal::Table, "Terrain document cannot be loaded in literal mode");
            auto candidate = WorldProof::LiteralOwner::from(source, document_keys_);
            documents_[selected_map_].swap(candidate);
        }
    }
}

std::optional<DemoTilemapEditor::Geometry> DemoTilemapEditor::geometry(const Sizes sizes, const Maps::LocalPoint pan, const double zoom) {
    if (sizes.logical.width <= 0 || sizes.logical.height <= 0 || sizes.framebuffer.width <= 0 || sizes.framebuffer.height <= 0 ||
        !representable(pan.x) || !representable(pan.y) || !representable(zoom) || zoom <= 0)
        return {};
    const auto scale = static_cast<float>(zoom);
    if (scale <= 0)
        return {};
    Geometry result;
    result.projection = glm::ortho(0.0f, static_cast<float>(sizes.framebuffer.width), 0.0f, static_cast<float>(sizes.framebuffer.height));
    result.view = glm::mat4(1);
    result.view[0][0] = result.view[1][1] = scale;
    const auto tx = -pan.x * zoom;
    const auto ty = -pan.y * zoom;
    if (!representable(tx) || !representable(ty))
        return {};
    result.view[3][0] = static_cast<float>(tx);
    result.view[3][1] = static_cast<float>(ty);
    if (!finite(result.projection) || !finite(result.view))
        return {};
    const auto right = pan.x + sizes.framebuffer.width / zoom;
    const auto top = pan.y + sizes.framebuffer.height / zoom;
    for (std::size_t i = 0; i < origins.size(); ++i) {
        const auto origin = origins[i];
        const Maps::LocalRect visible{pan.x - origin.x, origin.y - top, right - origin.x, origin.y - pan.y};
        if (!std::isfinite(visible.left) || !std::isfinite(visible.top) || !std::isfinite(visible.right) ||
            !std::isfinite(visible.bottom) || visible.left >= visible.right || visible.top >= visible.bottom)
            return {};
        result.maps[i] = Submit::TilemapView{.origin = {origin.x, origin.y}, .cell_size = cells, .visible = visible};
    }
    return result;
}

bool DemoTilemapEditor::set_view(const Maps::LocalPoint pan, const double zoom) {
    auto candidate = geometry(sizes_, pan, zoom);
    if (!candidate)
        return false;
    pan_ = pan;
    zoom_ = zoom;
    geometry_ = std::move(candidate);
    return true;
}

std::optional<Maps::Point> DemoTilemapEditor::pick(const Events::Pointer pointer) const {
    if (!geometry_ || !std::isfinite(pointer.x) || !std::isfinite(pointer.y) || pointer.x < 0 || pointer.y < 0 ||
        pointer.x >= sizes_.logical.width || pointer.y >= sizes_.logical.height)
        return {};
    // Full framebuffer viewport [0,w] x [0,h], Y-up. Scale axes independently;
    // never substitute a final cursor/axis sample for this observation's pointer.
    const auto x = pointer.x * sizes_.framebuffer.width / sizes_.logical.width;
    const auto y = sizes_.framebuffer.height - pointer.y * sizes_.framebuffer.height / sizes_.logical.height;
    const auto origin = origins[selected_map_];
    const Maps::LocalPoint local{pan_.x + x / zoom_ - origin.x, origin.y - (pan_.y + y / zoom_)};
    if (!std::isfinite(local.x) || !std::isfinite(local.y))
        return {};
    return Maps::pick_cell(map(selected_map_).extent(), cells, local);
}

void DemoTilemapEditor::consume(const std::span<const Events::Record> records, const Sizes sizes) {
    sizes_ = sizes;
    geometry_ = geometry(sizes_, pan_, zoom_);
    if (!geometry_)
        return;
    std::vector<Maps::TerrainEdit> pending;
    const auto flush = [&] {
        if (!pending.empty()) {
            static_cast<void>(terrain_[selected_map_]->edit_batch(selected_layer_, pending, terrain_limits_));
            pending.clear();
        }
    };
    for (const auto& record : records) {
        if (!record.to_gameplay)
            continue;
        if (const auto* button = std::get_if<Events::Button>(&record.data)) {
            if (button->phase != Events::ButtonPhase::Press)
                continue;
            if (record.device_kind == CE::Input::DeviceKind::Keyboard) {
                flush(); // Selection, modes, brushes and view changes affect only later records.
                using Key = CE::Input::KeyboardKey;
                switch (button->key) {
                    case Key::F8:
                        document_action(true);
                        break;
                    case Key::F9:
                        document_action(false);
                        break;
                    case Key::B:
                        mode_ = Mode::Terrain;
                        break;
                    case Key::V:
                        mode_ = Mode::Literal;
                        break;
                    case Key::Digit1:
                        selected_map_ = 0;
                        break;
                    case Key::Digit2:
                        selected_map_ = 1;
                        break;
                    case Key::G:
                        selected_layer_ = {0};
                        break;
                    case Key::T:
                        selected_layer_ = {1};
                        break;
                    case Key::Digit3:
                        if (mode_ == Mode::Terrain)
                            terrain_brush_ = {1};
                        else
                            brush_ = {7};
                        break;
                    case Key::Digit4:
                        if (mode_ == Mode::Terrain)
                            terrain_brush_ = {2};
                        else
                            brush_ = {8};
                        break;
                    case Key::Digit5:
                        if (mode_ == Mode::Terrain)
                            terrain_brush_ = {3};
                        else
                            brush_ = {9};
                        break;
                    case Key::Digit6:
                        if (mode_ == Mode::Terrain)
                            terrain_brush_ = {4};
                        else
                            brush_ = {10};
                        break;
                    case Key::A:
                        static_cast<void>(set_view({pan_.x - 32, pan_.y}, zoom_));
                        break;
                    case Key::D:
                        static_cast<void>(set_view({pan_.x + 32, pan_.y}, zoom_));
                        break;
                    case Key::S:
                        static_cast<void>(set_view({pan_.x, pan_.y - 32}, zoom_));
                        break;
                    case Key::W:
                        static_cast<void>(set_view({pan_.x, pan_.y + 32}, zoom_));
                        break;
                    case Key::R:
                        static_cast<void>(set_view({0, 0}, 1));
                        break;
                    default:
                        break;
                }
            } else if (record.device_kind == CE::Input::DeviceKind::Mouse && button->position) {
                const auto cell = pick(*button->position);
                if (!cell)
                    continue;
                auto& map = mutable_map(selected_map_);
                using Mouse = CE::Input::MouseButton;
                if (mode_ == Mode::Terrain) {
                    if (button->mouse_button == Mouse::Middle) {
                        flush();
                        terrain_brush_ = terrain_[selected_map_]->terrain(selected_layer_).label(*cell);
                    } else if (button->mouse_button == Mouse::Left || button->mouse_button == Mouse::Right) {
                        if (pending.size() == terrain_limits_.requests)
                            throw CE::Exceptions::bad_request(CE_HERE, "Private terrain input run exceeds request cap");
                        pending.push_back({*cell, button->mouse_button == Mouse::Left ? terrain_brush_ : Maps::TerrainLabel{0}});
                    }
                    continue;
                }
                switch (button->mouse_button) {
                    case Mouse::Left:
                        map.set(selected_layer_, *cell, brush_);
                        break;
                    case Mouse::Right:
                        map.set(selected_layer_, *cell, {0});
                        break;
                    case Mouse::Middle:
                        brush_ = map.get(selected_layer_, *cell);
                        break;
                    default:
                        break;
                }
            }
        } else if (const auto* scroll = std::get_if<Events::Scroll>(&record.data);
                   scroll && record.device_kind == CE::Input::DeviceKind::Mouse && scroll->position && std::isfinite(scroll->x) &&
                   std::isfinite(scroll->y)) {
            const auto pointer = *scroll->position;
            if (!std::isfinite(pointer.x) || !std::isfinite(pointer.y) || pointer.x < 0 || pointer.y < 0 ||
                pointer.x >= sizes_.logical.width || pointer.y >= sizes_.logical.height)
                continue;
            flush();
            const auto factor = std::exp2(scroll->y / 4);
            const auto zoom = zoom_ * factor;
            if (!std::isfinite(zoom) || zoom <= 0)
                continue;
            const auto x = pointer.x * sizes_.framebuffer.width / sizes_.logical.width;
            const auto y = sizes_.framebuffer.height - pointer.y * sizes_.framebuffer.height / sizes_.logical.height;
            // Keep the world point under THIS wheel record stationary.
            static_cast<void>(set_view({pan_.x + x / zoom_ - x / zoom, pan_.y + y / zoom_ - y / zoom}, zoom));
        }
    }
    flush();
}

void DemoTilemapEditor::write(
    CE::Rendering::FrameWriter& frame,
    const Submit::TilemapBindings& bindings,
    const CE::Rendering::DrawStyle2D& images
) const {
    if (!geometry_)
        return;
    auto pass = frame.begin_pass(geometry_->projection, geometry_->view);
    const Submit::Context2D context{pass.semantics(), pass.parameters(), pass.constraints(), Submit::ImageParameter2D{"image", 0}};
    auto overlay = images;
    overlay.alpha = 0.5f;
    // Explicit west ground/detail, east ground/detail, then each vector's row/column order.
    for (std::size_t i = 0; i < documents_.size(); ++i) {
        const auto& ground = mode_ == Mode::Terrain ? terrain_[i]->terrain({0}).tiles() : map(i);
        const auto& detail = mode_ == Mode::Terrain ? terrain_[i]->terrain({1}).tiles() : map(i);
        pass.add(Submit::resolve_tilemap_layer(ground, {0}, bindings, geometry_->maps[i], images, context));
        pass.add(Submit::resolve_tilemap_layer(detail, {1}, bindings, geometry_->maps[i], overlay, context));
    }
}
