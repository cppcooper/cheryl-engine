#include "asset-demo.h"

#include <core/resources/asset-management/manifest-loader.h>
#include <core/resources/asset-management/sprite-mgr.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <core/resources/asset-management/tileset-mgr.h>

#include <ext/matrix_transform.hpp>

#include <algorithm>
#include <exception>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
    constexpr float sample_scale = 3.0f;
    constexpr std::array<float, 4> columns{32.0f, 252.0f, 472.0f, 692.0f};
    constexpr std::array<CE::Assets::CellIndex, 6> static_tiles{0, 1, 2, 27, 28, 29};
    constexpr std::array<CE::Assets::CellIndex, 3> animated_tiles{309, 314, 324};
    constexpr std::array<std::string_view, 4> headings{
        "Static tiles", "Animated tiles\n400 / 200 / 100 ms", "Static sprites", "Animated sprites\nWalk / idle / attack"};
    constexpr std::size_t attack_clip = 5;

    template <typename Load> auto optional_asset(const std::string_view name, Load load) {
        try {
            return load();
        } catch (const std::exception& error) {
            std::cerr << "Optional demo " << name << " skipped: " << error.what() << '\n';
            return decltype(load()){};
        }
    }

    template <typename Definition>
    const Definition& entry(const std::vector<Definition>& definitions, const std::string_view name) {
        const auto found = std::ranges::find(definitions, name, &Definition::name);
        if (found == definitions.end())
            throw std::runtime_error("Missing manifest entry: " + std::string(name));
        return *found;
    }

    std::shared_ptr<const CE::Assets::Sprite>
    load_sprite(const CE::Assets::AssetManifest& manifest, const std::string_view name, CE::Assets::ResourceProvider& provider) {
        const auto& definition = entry(manifest.sprites, name);
        static_cast<void>(definition.grid.cell_rect(0));
        CE::Assets::TextureMgr::get().load_assets({definition.texture}, provider);
        auto& manager = CE::Assets::SpriteMgr::get();
        manager.load_assets({definition}, provider);
        auto sprite = manager.get_asset(definition.id());
        if (!sprite)
            throw std::runtime_error("Sprite was not published: " + definition.id());
        return sprite;
    }

    // Place the bottom-left of each sample consistently while respecting the
    // manifest's pivot. Grid geometry already converts image coordinates to Y-up.
    template <typename Definition>
    CE::RenderAPIs::DrawStyle2D placed(const CE::RenderAPIs::DrawStyle2D& base, const Definition& definition, float x, float y) {
        auto style = base;
        style.scale = sample_scale;
        x += definition.pivot.x * static_cast<float>(definition.grid.frame.width) * sample_scale;
        y += (1.0f - definition.pivot.y) * static_cast<float>(definition.grid.frame.height) * sample_scale;
        style.model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
        return style;
    }
}

void DemoAssets::load(
    const std::filesystem::path& root,
    CE::Assets::ResourceProvider& provider,
    const CE::Text::FontCollection& fonts
) {
    reset();
    // Parse package metadata independently. One package, sheet or clip failure
    // cannot suppress samples from a different available entry.
    tiles_ = optional_asset("Puny World tiles", [&]() -> std::shared_ptr<const CE::Assets::Tileset> {
        const auto manifest = CE::Assets::ManifestLoader::load(root / "punyworld-overworld.json");
        const auto& definition = entry(manifest.tilesets, "overworld");
        for (const auto cell : static_tiles)
            static_cast<void>(definition.grid.cell_rect(cell));
        for (const auto target : animated_tiles) {
            const auto& clip = definition.animations.at("tile-" + std::to_string(target));
            if (clip.target != target)
                throw std::runtime_error("Tile animation target does not match the demo sample");
        }
        CE::Assets::TextureMgr::get().load_assets({definition.texture}, provider);
        auto& manager = CE::Assets::TilesetMgr::get();
        manager.load_assets({definition}, provider);
        auto tiles = manager.get_asset(definition.id());
        if (!tiles)
            throw std::runtime_error("Tileset was not published: " + definition.id());
        for (const auto target : animated_tiles)
            static_cast<void>(tiles->cell_at(target, std::chrono::milliseconds{0}));
        return tiles;
    });
    const auto miniworld = optional_asset("MiniWorld manifest", [&] {
        return std::make_optional(CE::Assets::ManifestLoader::load(root / "atlas.json"));
    });
    if (miniworld) {
        weapon_ = optional_asset("static weapon", [&] { return load_sprite(*miniworld, "shortsword", provider); });
        character_ = optional_asset("Swordsman", [&]() -> std::optional<Character> {
            const auto sprite = load_sprite(*miniworld, "soldier_swordsman_cyan", provider);
            return Character{sprite, {sprite->animation("walk", "south"), sprite->animation("walk", "north"),
                sprite->animation("walk", "east"), sprite->animation("walk", "west"), sprite->animation("idle", "south"),
                sprite->animation("attack", "south")}};
        });
    }
    CE::Text::LayoutOptions options;
    options.pixel_height = 20;
    for (std::size_t i = 0; i < labels_.size(); ++i)
        labels_[i] = std::make_shared<const CE::Assets::RenderedText>(CE::Assets::upload_text(
            CE::Assets::prepare_text(CE::Text::layout_text(fonts, headings[i], options)), provider
        ));
}

void DemoAssets::reset() {
    labels_ = {};
    character_.reset();
    weapon_.reset();
    tiles_.reset();
    elapsed_ = {};
    paused_ = false;
    visible_ = true;
}

void DemoAssets::replay_attack() {
    if (character_)
        character_->clips[attack_clip].set_frame(0);
}

void DemoAssets::advance(const std::chrono::duration<double> delta) {
    if (paused_)
        return;
    elapsed_ += delta;
    if (character_)
        for (auto& clip : character_->clips)
            clip.advance(delta);
}

void DemoAssets::write(
    CE::RenderAPIs::RenderPassWriter& pass,
    const CE::RenderAPIs::DrawStyle2D& images,
    const CE::RenderAPIs::DrawStyle2D& text,
    const CE::Assets::SubmissionContext2D& context
) const {
    if (!visible_)
        return;
    for (std::size_t i = 0; i < labels_.size(); ++i) {
        if (!labels_[i])
            continue;
        auto label = text;
        label.model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(columns[i], 188.0f, 0.0f));
        pass.add(CE::Assets::resolve_text(*labels_[i], label, context));
    }
    if (tiles_) {
        for (std::size_t i = 0; i < static_tiles.size(); ++i) {
            const auto sample = placed(images, tiles_->definition(), columns[0] + static_cast<float>(i % 3) * 48.0f,
                28.0f + static_cast<float>(i / 3) * 48.0f);
            pass.add(CE::Assets::resolve_tile(*tiles_, static_tiles[i], sample, context));
        }
        const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_);
        for (std::size_t i = 0; i < animated_tiles.size(); ++i) {
            const auto sample = placed(images, tiles_->definition(), columns[1] + static_cast<float>(i) * 60.0f, 88.0f);
            pass.add(CE::Assets::resolve_tile(*tiles_, tiles_->cell_at(animated_tiles[i], time), sample, context));
        }
    }
    if (weapon_)
        pass.add(CE::Assets::resolve_sprite(*weapon_, 0, placed(images, weapon_->definition(), columns[2], 88.0f), context));
    if (character_) {
        const auto& sprite = *character_->sprite;
        pass.add(CE::Assets::resolve_sprite(sprite, 0, placed(images, sprite.definition(), columns[2] + 60.0f, 88.0f), context));
        for (std::size_t i = 0; i < character_->clips.size(); ++i) {
            const auto sample = placed(images, sprite.definition(), columns[3] + static_cast<float>(i % 3) * 60.0f,
                88.0f - static_cast<float>(i / 3) * 60.0f);
            pass.add(CE::Assets::resolve_sprite(sprite, character_->clips[i].cell(), sample, context));
        }
    }
}

std::string DemoAssets::status() const {
    return std::format("Tiles: {}  Swordsman: {}  Weapon: {}\nF7: show/hide samples  P: {}  Space: replay attack",
        tiles_ ? "ready" : "skipped", character_ ? "ready" : "skipped", weapon_ ? "ready" : "skipped", paused_ ? "resume" : "pause");
}
