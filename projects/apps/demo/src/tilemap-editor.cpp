#include "tilemap-editor.h"

#include <ext/matrix_clip_space.hpp>

#include <cmath>
#include <limits>

namespace {
    namespace Maps = CE::Tilemaps;
    namespace Submit = CE::Rendering::Submission;
    namespace Events = CE::Input::Events;
    constexpr std::array<Maps::LocalPoint, 2> origins{Maps::LocalPoint{48, 240}, Maps::LocalPoint{328, 208}};
    constexpr Maps::CellSize cells{64, 48};

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

DemoTilemapEditor::DemoTilemapEditor() {
    constexpr std::array<std::array<std::uint32_t, 6>, 4> ids{
        {{7, 8, 9, 10, 0, 7}, {0, 10, 0, 9, 0, 0}, {9, 7, 0, 8, 10, 9}, {10, 0, 10, 0, 7, 0}}
    };
    for (std::size_t instance = 0; instance < maps_.size(); ++instance)
        for (std::int64_t layer = 0; layer < 2; ++layer)
            for (std::int64_t row = 0; row < 2; ++row)
                for (std::int64_t column = 0; column < 3; ++column)
                    maps_[instance].set({layer}, {column, row}, {ids[instance * 2 + layer][row * 3 + column]});
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
    return Maps::pick_cell(maps_[selected_map_].extent(), cells, local);
}

void DemoTilemapEditor::consume(const std::span<const Events::Record> records, const Sizes sizes) {
    sizes_ = sizes;
    geometry_ = geometry(sizes_, pan_, zoom_);
    if (!geometry_)
        return;
    for (const auto& record : records) {
        if (!record.to_gameplay)
            continue;
        if (const auto* button = std::get_if<Events::Button>(&record.data)) {
            if (button->phase != Events::ButtonPhase::Press)
                continue;
            if (record.device_kind == CE::Input::DeviceKind::Keyboard) {
                using Key = CE::Input::KeyboardKey;
                switch (button->key) {
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
                        brush_ = {7};
                        break;
                    case Key::Digit4:
                        brush_ = {8};
                        break;
                    case Key::Digit5:
                        brush_ = {9};
                        break;
                    case Key::Digit6:
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
                auto& map = maps_[selected_map_];
                using Mouse = CE::Input::MouseButton;
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
    for (std::size_t i = 0; i < maps_.size(); ++i) {
        pass.add(Submit::resolve_tilemap_layer(maps_[i], {0}, bindings, geometry_->maps[i], images, context));
        pass.add(Submit::resolve_tilemap_layer(maps_[i], {1}, bindings, geometry_->maps[i], overlay, context));
    }
}
