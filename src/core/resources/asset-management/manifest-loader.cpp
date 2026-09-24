#include <core/resources/asset-management/manifest-loader.h>

#include <nlohmann/json.hpp>
#include <internals/exceptions.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace CE::Assets {
    namespace {
        using json = nlohmann::json;
        namespace fs = std::filesystem;

        constexpr std::array directions{
            std::pair{"north", Direction::North}, std::pair{"north_east", Direction::NorthEast},
            std::pair{"east", Direction::East},   std::pair{"south_east", Direction::SouthEast},
            std::pair{"south", Direction::South}, std::pair{"south_west", Direction::SouthWest},
            std::pair{"west", Direction::West},   std::pair{"north_west", Direction::NorthWest}};

        [[noreturn]] void fail(const fs::path& source, const std::string_view location,
                               const std::string_view message) {
            throw Exceptions::runtime_exception(CE_HERE,
                                                "Asset manifest '" + source.string() + "' at " + std::string(location) +
                                                    ": " + std::string(message));
        }

        void require_object(const json& value, const fs::path& source, const std::string_view location) {
            if (!value.is_object()) {
                fail(source, location, "expected an object");
            }
        }

        void require_array(const json& value, const fs::path& source, const std::string_view location) {
            if (!value.is_array()) {
                fail(source, location, "expected an array");
            }
        }

        void allow_only(const json& object, const fs::path& source, const std::string_view location,
                        const std::initializer_list<std::string_view> allowed) {
            require_object(object, source, location);
            for (const auto& [key, value] : object.items()) {
                static_cast<void>(value);
                if (std::ranges::find(allowed, key) == allowed.end()) {
                    fail(source, location, "unknown property '" + key + "'");
                }
            }
        }

        const json& required(const json& object, const std::string_view key, const fs::path& source,
                             const std::string_view location) {
            require_object(object, source, location);
            const auto iter = object.find(key);
            if (iter == object.end()) {
                fail(source, location, "missing required property '" + std::string(key) + "'");
            }
            return *iter;
        }

        std::string read_string(const json& value, const fs::path& source, const std::string_view location,
                                const bool allow_empty = false) {
            if (!value.is_string()) {
                fail(source, location, "expected a string");
            }
            auto result = value.get<std::string>();
            if (!allow_empty && result.empty()) {
                fail(source, location, "expected a non-empty string");
            }
            return result;
        }

        std::string description(const json& object, const fs::path& source, const std::string_view location) {
            if (!object.contains("description")) {
                return {};
            }
            return read_string(object.at("description"), source, std::string(location) + ".description");
        }

        std::uint64_t read_unsigned(const json& value, const fs::path& source, const std::string_view location,
                                    const std::uint64_t minimum = 0,
                                    const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max()) {
            if (!value.is_number_integer() && !value.is_number_unsigned()) {
                fail(source, location, "expected an integer");
            }
            if (value.type() == json::value_t::number_integer && value.get<std::int64_t>() < 0) {
                fail(source, location, "expected a non-negative integer");
            }
            const auto result = value.get<std::uint64_t>();
            if (result < minimum || result > maximum) {
                fail(source, location, "integer is outside the supported range");
            }
            return result;
        }

        std::size_t read_size(const json& value, const fs::path& source, const std::string_view location,
                              const std::size_t minimum = 0) {
            return static_cast<std::size_t>(
                read_unsigned(value, source, location, minimum, std::numeric_limits<std::size_t>::max()));
        }

        std::uint32_t read_u32(const json& value, const fs::path& source, const std::string_view location,
                               const std::uint32_t minimum = 0) {
            return static_cast<std::uint32_t>(
                read_unsigned(value, source, location, minimum, std::numeric_limits<std::uint32_t>::max()));
        }

        double read_positive_number(const json& value, const fs::path& source, const std::string_view location) {
            if (!value.is_number()) {
                fail(source, location, "expected a number");
            }
            const auto result = value.get<double>();
            if (!std::isfinite(result) || result <= 0.0) {
                fail(source, location, "expected a finite number greater than zero");
            }
            return result;
        }

        std::chrono::milliseconds read_duration(const json& value, const fs::path& source,
                                                const std::string_view location) {
            const auto count = read_unsigned(value, source, location, 1, std::numeric_limits<std::int64_t>::max());
            return std::chrono::milliseconds(static_cast<std::int64_t>(count));
        }

        bool is_identifier(const std::string_view value, const bool allow_dot) {
            if (value.empty() ||
                !((value.front() >= 'a' && value.front() <= 'z') || (value.front() >= '0' && value.front() <= '9'))) {
                return false;
            }
            return std::ranges::all_of(value, [allow_dot](const char ch) {
                return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' ||
                    (allow_dot && ch == '.');
            });
        }

        std::string read_identifier(const json& value, const fs::path& source, const std::string_view location,
                                    const bool allow_dot = false) {
            auto result = read_string(value, source, location);
            if (!is_identifier(result, allow_dot)) {
                fail(source, location, "value is not a valid lowercase identifier");
            }
            return result;
        }

        void validate_map_key(const std::string& key, const fs::path& source, const std::string_view location) {
            if (!is_identifier(key, false)) {
                fail(source, location, "map key '" + key + "' is not a valid identifier");
            }
        }

        math::Pivot parse_pivot(const json& value, const fs::path& source, const std::string_view location) {
            allow_only(value, source, location, {"x", "y"});
            const auto parse_component = [&](const std::string_view key) {
                const auto& component = required(value, key, source, location);
                if (!component.is_number()) {
                    fail(source, std::string(location) + '.' + std::string(key), "expected a number");
                }
                const auto result = component.get<float>();
                if (!std::isfinite(result) || result < 0.0f || result > 1.0f) {
                    fail(source, std::string(location) + '.' + std::string(key),
                         "pivot components must be in the [0, 1] range");
                }
                return result;
            };
            return {parse_component("x"), parse_component("y")};
        }

        fs::path parse_texture_path(const json& value, const fs::path& source, const std::string_view location) {
            // Resolve only after rejecting absolute paths and parent traversal, so every
            // accepted reference has a lexical path under the manifest's directory.
            const auto text = read_string(value, source, location);
            if (text.contains('\\')) {
                fail(source, location, "texture paths must use forward slashes");
            }
            const fs::path relative(text);
            if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory()) {
                fail(source, location, "texture path must be relative to the manifest");
            }
            for (const auto& part : relative) {
                if (part == "..") {
                    fail(source, location, "texture path cannot traverse to a parent directory");
                }
            }
            return (source.parent_path() / relative).lexically_normal();
        }

        GridDefinition parse_grid(const json& value, const fs::path& source, const std::string_view location) {
            allow_only(value, source, location, {"origin", "frame", "spacing", "rows", "columns", "cell_order"});

            const auto& origin = required(value, "origin", source, location);
            allow_only(origin, source, std::string(location) + ".origin", {"x", "y"});
            const auto& frame = required(value, "frame", source, location);
            allow_only(frame, source, std::string(location) + ".frame", {"width", "height"});
            const auto& spacing = required(value, "spacing", source, location);
            allow_only(spacing, source, std::string(location) + ".spacing", {"x", "y"});

            GridDefinition result{.origin = {read_u32(required(origin, "x", source, location), source,
                                                      std::string(location) + ".origin.x"),
                                             read_u32(required(origin, "y", source, location), source,
                                                      std::string(location) + ".origin.y")},
                                  .frame = {read_u32(required(frame, "width", source, location), source,
                                                     std::string(location) + ".frame.width", 1),
                                            read_u32(required(frame, "height", source, location), source,
                                                     std::string(location) + ".frame.height", 1)},
                                  .spacing = {read_u32(required(spacing, "x", source, location), source,
                                                       std::string(location) + ".spacing.x"),
                                              read_u32(required(spacing, "y", source, location), source,
                                                       std::string(location) + ".spacing.y")},
                                  .rows = read_size(required(value, "rows", source, location), source,
                                                    std::string(location) + ".rows", 1),
                                  .columns = read_size(required(value, "columns", source, location), source,
                                                       std::string(location) + ".columns", 1)};

            // Keep the parser's addressing convention and renderer vertex limit aligned before
            // any named cell, animation, or autotile is expanded against this grid.
            if (read_string(required(value, "cell_order", source, location), source,
                            std::string(location) + ".cell_order") != "row-major") {
                fail(source, std::string(location) + ".cell_order", "only row-major grids are supported");
            }
            const auto cells = result.cell_count();
            if (cells > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) / 4) {
                fail(source, location, "grid has too many cells for the renderer");
            }
            return result;
        }

        CellIndex parse_cell(const json& value, const GridDefinition& grid, const fs::path& source,
                             const std::string_view location) {
            require_object(value, source, location);
            // The two JSON forms converge on one row-major index; validate against the
            // grid before storing it in a view, animation, or autotile definition.
            if (value.contains("index")) {
                allow_only(value, source, location, {"index"});
                const auto index = read_size(value.at("index"), source, std::string(location) + ".index");
                if (index >= grid.cell_count()) {
                    fail(source, location, "cell index is outside the grid");
                }
                return index;
            }
            allow_only(value, source, location, {"row", "column"});
            const auto row =
                read_size(required(value, "row", source, location), source, std::string(location) + ".row");
            const auto column =
                read_size(required(value, "column", source, location), source, std::string(location) + ".column");
            if (row >= grid.rows || column >= grid.columns) {
                fail(source, location, "cell coordinates are outside the grid");
            }
            return grid.cell_index(row, column);
        }

        std::unordered_map<std::string, ViewDefinition> parse_views(const json& value, const GridDefinition& grid,
                                                                    const fs::path& source,
                                                                    const std::string_view location) {
            require_object(value, source, location);
            if (value.empty()) {
                fail(source, location, "view map cannot be empty");
            }
            std::unordered_map<std::string, ViewDefinition> result;
            for (const auto& [name, view] : value.items()) {
                validate_map_key(name, source, location);
                const auto view_location = std::string(location) + '.' + name;
                allow_only(view, source, view_location, {"description", "row", "column", "rows", "columns"});
                ViewDefinition definition{
                    .description = description(view, source, view_location),
                    .row = read_size(required(view, "row", source, view_location), source, view_location + ".row"),
                    .column =
                        read_size(required(view, "column", source, view_location), source, view_location + ".column"),
                    .rows =
                        read_size(required(view, "rows", source, view_location), source, view_location + ".rows", 1),
                    .columns = read_size(required(view, "columns", source, view_location), source,
                                         view_location + ".columns", 1)};
                // Check the starting cell before subtracting it from the grid dimensions;
                // the remaining span then bounds the rectangle without addition overflow.
                if (definition.row >= grid.rows || definition.column >= grid.columns ||
                    definition.rows > grid.rows - definition.row ||
                    definition.columns > grid.columns - definition.column) {
                    fail(source, view_location, "view rectangle is outside the grid");
                }
                result.emplace(name, std::move(definition));
            }
            return result;
        }

        std::unordered_map<std::string, CellIndex> parse_orientations(const json& value, const GridDefinition& grid,
                                                                      const fs::path& source,
                                                                      const std::string_view location) {
            require_object(value, source, location);
            if (value.empty()) {
                fail(source, location, "orientation map cannot be empty");
            }
            std::unordered_map<std::string, CellIndex> result;
            for (const auto& [name, cell] : value.items()) {
                validate_map_key(name, source, location);
                result.emplace(name, parse_cell(cell, grid, source, std::string(location) + '.' + name));
            }
            return result;
        }

        std::vector<TimedFrameDefinition> parse_frames(const json& value, const GridDefinition& grid,
                                                       const fs::path& source, const std::string_view location) {
            require_array(value, source, location);
            if (value.empty()) {
                fail(source, location, "animation must contain at least one frame");
            }
            std::vector<TimedFrameDefinition> result;
            result.reserve(value.size());
            for (std::size_t index = 0; index < value.size(); ++index) {
                const auto frame_location = std::string(location) + '[' + std::to_string(index) + ']';
                const auto& frame = value[index];
                allow_only(frame, source, frame_location, {"cell", "duration_ms"});
                result.push_back({parse_cell(required(frame, "cell", source, frame_location), grid, source,
                                             frame_location + ".cell"),
                                  read_duration(required(frame, "duration_ms", source, frame_location), source,
                                                frame_location + ".duration_ms")});
            }
            return result;
        }

        std::unordered_map<std::string, AnimationProfileDefinition>
        parse_profiles(const json& value, const fs::path& source, const std::string_view location) {
            require_object(value, source, location);
            if (value.empty()) {
                fail(source, location, "animation profile map cannot be empty");
            }
            std::unordered_map<std::string, AnimationProfileDefinition> result;
            for (const auto& [profile_name, profile] : value.items()) {
                validate_map_key(profile_name, source, location);
                const auto profile_location = std::string(location) + '.' + profile_name;
                allow_only(profile, source, profile_location, {"description", "facings", "clips"});
                AnimationProfileDefinition definition;
                definition.description = description(profile, source, profile_location);

                // Profiles are grid-independent: preserve facing row offsets here and
                // resolve them against each sprite's own grid during expansion.
                const auto& facings = required(profile, "facings", source, profile_location);
                require_object(facings, source, profile_location + ".facings");
                if (facings.empty()) {
                    fail(source, profile_location + ".facings", "facing map cannot be empty");
                }
                for (const auto& [name, offset] : facings.items()) {
                    validate_map_key(name, source, profile_location + ".facings");
                    definition.facings.emplace(name, read_size(offset, source, profile_location + ".facings." + name));
                }

                const auto& clips = required(profile, "clips", source, profile_location);
                require_object(clips, source, profile_location + ".clips");
                if (clips.empty()) {
                    fail(source, profile_location + ".clips", "clip map cannot be empty");
                }
                for (const auto& [name, clip] : clips.items()) {
                    validate_map_key(name, source, profile_location + ".clips");
                    const auto clip_location = profile_location + ".clips." + name;
                    allow_only(clip, source, clip_location,
                               {"description", "row_offset", "columns", "frame_duration_ms", "loop"});
                    ProfileClipDefinition clip_definition;
                    clip_definition.description = description(clip, source, clip_location);
                    clip_definition.row_offset = read_size(required(clip, "row_offset", source, clip_location), source,
                                                           clip_location + ".row_offset");
                    const auto& columns = required(clip, "columns", source, clip_location);
                    require_array(columns, source, clip_location + ".columns");
                    if (columns.empty()) {
                        fail(source, clip_location + ".columns", "column list cannot be empty");
                    }
                    // Preserve frame order while forbidding duplicate columns within a clip;
                    // the same ordered columns will be reused for every facing.
                    std::unordered_set<std::size_t> unique_columns;
                    for (std::size_t index = 0; index < columns.size(); ++index) {
                        const auto column = read_size(columns[index], source,
                                                      clip_location + ".columns[" + std::to_string(index) + ']');
                        if (!unique_columns.emplace(column).second) {
                            fail(source, clip_location + ".columns", "column values must be unique");
                        }
                        clip_definition.columns.push_back(column);
                    }
                    clip_definition.frame_duration =
                        read_duration(required(clip, "frame_duration_ms", source, clip_location), source,
                                      clip_location + ".frame_duration_ms");
                    const auto& loop = required(clip, "loop", source, clip_location);
                    if (!loop.is_boolean()) {
                        fail(source, clip_location + ".loop", "expected a boolean");
                    }
                    clip_definition.loop = loop.get<bool>();
                    definition.clips.emplace(name, std::move(clip_definition));
                }
                result.emplace(profile_name, std::move(definition));
            }
            return result;
        }

        void append_profile_animations(SpriteDefinition& sprite, const AnimationProfileDefinition& profile,
                                       const fs::path& source, const std::string_view location) {
            // Expand each clip/facing combination into a concrete sequence of cells so runtime
            // sprite lookup does not need to interpret the profile's row offsets again.
            for (const auto& [clip_name, clip] : profile.clips) {
                for (const auto& [facing_name, facing_offset] : profile.facings) {
                    // A facing shifts the clip's row, whereas each frame selects a column;
                    // both must fit this particular sprite before an animation is emitted.
                    if (facing_offset > std::numeric_limits<std::size_t>::max() - clip.row_offset) {
                        fail(source, location, "animation profile row calculation overflowed");
                    }
                    const auto row = facing_offset + clip.row_offset;
                    if (row >= sprite.grid.rows) {
                        fail(source, location, "animation profile selects a row outside the sprite grid");
                    }
                    SpriteAnimationDefinition animation{.name = clip_name,
                                                        .facing = facing_name,
                                                        .description = clip.description,
                                                        .frames = {},
                                                        .loop = clip.loop};
                    animation.frames.reserve(clip.columns.size());
                    for (const auto column : clip.columns) {
                        if (column >= sprite.grid.columns) {
                            fail(source, location, "animation profile selects a column outside the sprite grid");
                        }
                        animation.frames.push_back({sprite.grid.cell_index(row, column), clip.frame_duration});
                    }
                    sprite.animations.push_back(std::move(animation));
                }
            }
        }

        void append_explicit_animations(SpriteDefinition& sprite, const json& value, const fs::path& source,
                                        const std::string_view location) {
            // Local clips carry their own cell sequence and have no facing; they share
            // the sprite grid validation used by profile-derived frame sequences.
            require_object(value, source, location);
            if (value.empty()) {
                fail(source, location, "animation map cannot be empty");
            }
            for (const auto& [name, clip] : value.items()) {
                validate_map_key(name, source, location);
                const auto clip_location = std::string(location) + '.' + name;
                allow_only(clip, source, clip_location, {"description", "frames", "loop"});
                const auto& loop = required(clip, "loop", source, clip_location);
                if (!loop.is_boolean()) {
                    fail(source, clip_location + ".loop", "expected a boolean");
                }
                sprite.animations.push_back({.name = name,
                                             .facing = std::nullopt,
                                             .description = description(clip, source, clip_location),
                                             .frames = parse_frames(required(clip, "frames", source, clip_location),
                                                                    sprite.grid, source, clip_location + ".frames"),
                                             .loop = loop.get<bool>()});
            }
        }

        Direction parse_direction(const json& value, const fs::path& source, const std::string_view location) {
            const auto name = read_string(value, source, location);
            const auto iter =
                std::ranges::find_if(directions, [&name](const auto& item) { return item.first == name; });
            if (iter == directions.end()) {
                fail(source, location, "unknown direction '" + name + "'");
            }
            return iter->second;
        }

        bool valid_color(const std::string_view color) {
            if (color.size() != 7 && color.size() != 9) {
                return false;
            }
            if (color.front() != '#') {
                return false;
            }
            return std::ranges::all_of(color.substr(1), [](const char ch) {
                return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
            });
        }

        WangAutotileDefinition parse_wang_autotile(const json& value, const GridDefinition& grid, const WangType type,
                                                   const fs::path& source, const std::string_view location) {
            allow_only(value, source, location, {"description", "type", "slot_order", "terrains", "tiles"});
            WangAutotileDefinition result{.description = description(value, source, location),
                                          .type = type,
                                          .slot_order = {},
                                          .terrains = {},
                                          .tiles = {},
                                          .variants = {}};

            // Fix the slot order first: alternating positions represent edges and corners,
            // which determines where nonzero terrain IDs are allowed below.
            const auto& slots = required(value, "slot_order", source, location);
            require_array(slots, source, std::string(location) + ".slot_order");
            if (slots.size() != result.slot_order.size()) {
                fail(source, std::string(location) + ".slot_order", "Wang slot order must have 8 entries");
            }
            for (std::size_t index = 0; index < result.slot_order.size(); ++index) {
                result.slot_order[index] = parse_direction(
                    slots[index], source, std::string(location) + ".slot_order[" + std::to_string(index) + ']');
                if (result.slot_order[index] != directions[index].second) {
                    fail(source, std::string(location) + ".slot_order",
                         "Wang slots must use the canonical north-to-north_west order");
                }
            }

            // Build the terrain ID set before reading tile signatures so a signature
            // cannot refer to a terrain missing from this autotile definition.
            const auto& terrains = required(value, "terrains", source, location);
            require_array(terrains, source, std::string(location) + ".terrains");
            if (terrains.empty()) {
                fail(source, std::string(location) + ".terrains", "terrain list cannot be empty");
            }
            std::unordered_set<std::uint32_t> terrain_ids;
            for (std::size_t index = 0; index < terrains.size(); ++index) {
                const auto terrain_location = std::string(location) + ".terrains[" + std::to_string(index) + ']';
                const auto& terrain = terrains[index];
                allow_only(terrain, source, terrain_location, {"id", "name", "color", "probability", "representative"});
                TerrainDefinition definition{.id = read_u32(required(terrain, "id", source, terrain_location), source,
                                                            terrain_location + ".id", 1),
                                             .name = read_string(required(terrain, "name", source, terrain_location),
                                                                 source, terrain_location + ".name"),
                                             .color = std::nullopt,
                                             .probability = 1.0,
                                             .representative = std::nullopt};
                if (!terrain_ids.emplace(definition.id).second) {
                    fail(source, terrain_location + ".id", "terrain IDs must be unique");
                }
                if (terrain.contains("color")) {
                    const auto color = read_string(terrain.at("color"), source, terrain_location + ".color");
                    if (!valid_color(color)) {
                        fail(source, terrain_location + ".color", "expected #RRGGBB or #RRGGBBAA");
                    }
                    definition.color = color;
                }
                if (terrain.contains("probability")) {
                    definition.probability =
                        read_positive_number(terrain.at("probability"), source, terrain_location + ".probability");
                }
                if (terrain.contains("representative")) {
                    definition.representative =
                        parse_cell(terrain.at("representative"), grid, source, terrain_location + ".representative");
                }
                result.terrains.push_back(std::move(definition));
            }

            // Index candidates by their complete signature; retain multiple weighted variants
            // instead of choosing one at parse time when neighboring terrain is still unknown.
            const auto& tiles = required(value, "tiles", source, location);
            require_array(tiles, source, std::string(location) + ".tiles");
            if (tiles.empty()) {
                fail(source, std::string(location) + ".tiles", "Wang tile list cannot be empty");
            }
            for (std::size_t index = 0; index < tiles.size(); ++index) {
                const auto tile_location = std::string(location) + ".tiles[" + std::to_string(index) + ']';
                const auto& tile = tiles[index];
                allow_only(tile, source, tile_location, {"cell", "wang", "weight"});
                WangTileDefinition definition{.cell = parse_cell(required(tile, "cell", source, tile_location), grid,
                                                                 source, tile_location + ".cell")};
                const auto& signature = required(tile, "wang", source, tile_location);
                require_array(signature, source, tile_location + ".wang");
                if (signature.size() != definition.wang.size()) {
                    fail(source, tile_location + ".wang", "Wang signature must have 8 entries");
                }
                // Zero leaves a slot unassigned; assigned IDs must both exist and
                // occupy the slots permitted by this edge or corner Wang type.
                for (std::size_t slot = 0; slot < definition.wang.size(); ++slot) {
                    definition.wang[slot] =
                        read_u32(signature[slot], source, tile_location + ".wang[" + std::to_string(slot) + ']');
                    if (definition.wang[slot] != 0 && !terrain_ids.contains(definition.wang[slot])) {
                        fail(source, tile_location + ".wang", "Wang signature references an unknown terrain");
                    }
                    const bool is_edge_slot = slot % 2 == 0;
                    if (definition.wang[slot] != 0 &&
                        ((type == WangType::Corner && is_edge_slot) || (type == WangType::Edge && !is_edge_slot))) {
                        fail(source, tile_location + ".wang", "Wang signature uses a slot incompatible with its type");
                    }
                }
                if (tile.contains("weight")) {
                    definition.weight = read_positive_number(tile.at("weight"), source, tile_location + ".weight");
                }
                const auto signature_value = definition.wang;
                const auto tile_index = result.tiles.size();
                result.tiles.push_back(std::move(definition));
                result.variants[signature_value].push_back(tile_index);
            }
            return result;
        }

        BitmaskAutotileDefinition parse_bitmask_autotile(const json& value, const GridDefinition& grid,
                                                         const BitmaskType type, const fs::path& source,
                                                         const std::string_view location) {
            allow_only(value, source, location, {"description", "type", "bit_order", "cases"});
            BitmaskAutotileDefinition result{
                .description = description(value, source, location), .type = type, .bit_order = {}, .cases = {}};
            const auto& order = required(value, "bit_order", source, location);
            require_array(order, source, std::string(location) + ".bit_order");
            if (order.empty() || order.size() > 8) {
                fail(source, std::string(location) + ".bit_order", "bit order must have 1 to 8 entries");
            }
            std::unordered_set<int> seen;
            // The declared direction at each position becomes that bit's meaning;
            // repeated directions would make distinct masks describe the same neighbors.
            for (std::size_t index = 0; index < order.size(); ++index) {
                const auto direction = parse_direction(
                    order[index], source, std::string(location) + ".bit_order[" + std::to_string(index) + ']');
                if (!seen.emplace(static_cast<int>(direction)).second) {
                    fail(source, std::string(location) + ".bit_order", "directions must be unique");
                }
                result.bit_order.push_back(direction);
            }
            // The declared neighbor order determines which bits a future tile-map selector sets.
            // Reject masks outside that width before storing their resolved cell indices.
            const auto& cases = required(value, "cases", source, location);
            require_object(cases, source, std::string(location) + ".cases");
            if (cases.empty()) {
                fail(source, std::string(location) + ".cases", "bitmask case map cannot be empty");
            }
            const auto maximum_mask = (std::uint32_t{1} << result.bit_order.size()) - 1;
            for (const auto& [mask_text, cell] : cases.items()) {
                // Demand the canonical decimal spelling so each numerical mask has one
                // JSON key, then reject any bits not declared by bit_order.
                std::uint32_t mask{};
                const auto [end, error] = std::from_chars(mask_text.data(), mask_text.data() + mask_text.size(), mask);
                if (error != std::errc{} || end != mask_text.data() + mask_text.size() ||
                    (mask_text.size() > 1 && mask_text.front() == '0') || mask > maximum_mask) {
                    fail(source, std::string(location) + ".cases." + mask_text, "invalid or out-of-range bitmask");
                }
                result.cases.emplace(mask,
                                     parse_cell(cell, grid, source, std::string(location) + ".cases." + mask_text));
            }
            return result;
        }

        std::unordered_map<std::string, AutotileDefinition> parse_autotiles(const json& value,
                                                                            const GridDefinition& grid,
                                                                            const fs::path& source,
                                                                            const std::string_view location) {
            require_object(value, source, location);
            if (value.empty()) {
                fail(source, location, "autotile map cannot be empty");
            }
            std::unordered_map<std::string, AutotileDefinition> result;
            for (const auto& [name, autotile] : value.items()) {
                validate_map_key(name, source, location);
                const auto autotile_location = std::string(location) + '.' + name;
                const auto type = read_string(required(autotile, "type", source, autotile_location), source,
                                              autotile_location + ".type");
                if (type == "wang-corner") {
                    result.emplace(name,
                                   parse_wang_autotile(autotile, grid, WangType::Corner, source, autotile_location));
                }
                else if (type == "wang-edge") {
                    result.emplace(name,
                                   parse_wang_autotile(autotile, grid, WangType::Edge, source, autotile_location));
                }
                else if (type == "four-neighbor") {
                    result.emplace(
                        name,
                        parse_bitmask_autotile(autotile, grid, BitmaskType::FourNeighbor, source, autotile_location));
                }
                else if (type == "eight-neighbor") {
                    result.emplace(
                        name,
                        parse_bitmask_autotile(autotile, grid, BitmaskType::EightNeighbor, source, autotile_location));
                }
                else {
                    fail(source, autotile_location + ".type", "unknown autotile type '" + type + "'");
                }
            }
            return result;
        }

        class Parser {
        public:
            explicit Parser(fs::path source) : source_(std::move(source)) {}

            AssetManifest parse(const json& root) const {
                // Establish document identity and defaults first; individual assets inherit these
                // values unless their entry provides a more specific texture or pivot.
                allow_only(root, source_, "$",
                           {"$schema", "version", "namespace", "texture", "defaults", "animation_profiles", "sprites",
                            "tilesets"});
                AssetManifest manifest;
                manifest.source = source_;
                manifest.schema = read_string(required(root, "$schema", source_, "$"), source_, "$.$schema");
                if (manifest.schema != "./schemas/asset-manifest-1.0.schema.json") {
                    fail(source_, "$.$schema", "unsupported asset manifest schema");
                }
                manifest.version = read_string(required(root, "version", source_, "$"), source_, "$.version");
                if (manifest.version != "1.0") {
                    fail(source_, "$.version", "unsupported asset manifest version '" + manifest.version + "'");
                }
                manifest.name_space =
                    read_identifier(required(root, "namespace", source_, "$"), source_, "$.namespace", true);

                const auto& defaults = required(root, "defaults", source_, "$");
                allow_only(defaults, source_, "$.defaults", {"sprite", "tileset"});
                const auto& sprite_defaults = required(defaults, "sprite", source_, "$.defaults");
                allow_only(sprite_defaults, source_, "$.defaults.sprite", {"pivot"});
                manifest.default_sprite_pivot =
                    parse_pivot(required(sprite_defaults, "pivot", source_, "$.defaults.sprite"), source_,
                                "$.defaults.sprite.pivot");
                const auto& tileset_defaults = required(defaults, "tileset", source_, "$.defaults");
                allow_only(tileset_defaults, source_, "$.defaults.tileset", {"pivot"});
                manifest.default_tileset_pivot =
                    parse_pivot(required(tileset_defaults, "pivot", source_, "$.defaults.tileset"), source_,
                                "$.defaults.tileset.pivot");

                if (root.contains("texture")) {
                    manifest.texture = parse_texture_path(root.at("texture"), source_, "$.texture");
                }
                // Profile parsing establishes reusable offsets; entry parsing below
                // expands them only after the referenced sprite grid is known.
                if (root.contains("animation_profiles")) {
                    manifest.animation_profiles =
                        parse_profiles(root.at("animation_profiles"), source_, "$.animation_profiles");
                }
                // Expand profiles and local grid references while parsing entries. Cross-manifest
                // identity and physical image bounds must wait for Loader's collection pass.
                if (!root.contains("sprites") && !root.contains("tilesets")) {
                    fail(source_, "$", "manifest must contain sprites or tilesets");
                }
                if (root.contains("sprites")) {
                    parse_sprites(root.at("sprites"), manifest);
                }
                if (root.contains("tilesets")) {
                    parse_tilesets(root.at("tilesets"), manifest);
                }
                return manifest;
            }

        private:
            fs::path source_;

            fs::path entry_texture(const json& entry, const AssetManifest& manifest,
                                   const std::string_view location) const {
                if (entry.contains("texture")) {
                    return parse_texture_path(entry.at("texture"), source_, std::string(location) + ".texture");
                }
                if (manifest.texture) {
                    return *manifest.texture;
                }
                fail(source_, location, "asset has no entry-level or manifest-level texture");
            }

            void parse_sprites(const json& value, AssetManifest& manifest) const {
                require_object(value, source_, "$.sprites");
                if (value.empty()) {
                    fail(source_, "$.sprites", "sprite map cannot be empty");
                }
                manifest.sprites.reserve(value.size());
                for (const auto& [name, sprite] : value.items()) {
                    validate_map_key(name, source_, "$.sprites");
                    const auto location = "$.sprites." + name;
                    allow_only(sprite, source_, location,
                               {"description", "texture", "grid", "pivot", "views", "orientations", "animation_profile",
                                "animations"});
                    // Bind an entry to its effective texture and pivot before resolving
                    // anything that addresses cells within its grid.
                    SpriteDefinition definition{
                        .name_space = manifest.name_space,
                        .name = name,
                        .description = description(sprite, source_, location),
                        .texture = entry_texture(sprite, manifest, location),
                        .grid = parse_grid(required(sprite, "grid", source_, location), source_, location + ".grid"),
                        .pivot = sprite.contains("pivot")
                            ? parse_pivot(sprite.at("pivot"), source_, location + ".pivot")
                            : manifest.default_sprite_pivot,
                        .views = {},
                        .orientations = {},
                        .animation_profile = std::nullopt,
                        .animations = {}};
                    // Named views and orientations are local to this grid, so reject
                    // out-of-range references while the owning entry is still in scope.
                    if (sprite.contains("views")) {
                        definition.views =
                            parse_views(sprite.at("views"), definition.grid, source_, location + ".views");
                    }
                    if (sprite.contains("orientations")) {
                        definition.orientations = parse_orientations(sprite.at("orientations"), definition.grid,
                                                                     source_, location + ".orientations");
                    }
                    // Profile clips produce facing-specific sequences; explicit clips append
                    // unfaced sequences. Sprite construction later checks for duplicate keys.
                    if (sprite.contains("animation_profile")) {
                        const auto profile_name =
                            read_identifier(sprite.at("animation_profile"), source_, location + ".animation_profile");
                        const auto profile = manifest.animation_profiles.find(profile_name);
                        if (profile == manifest.animation_profiles.end()) {
                            fail(source_, location + ".animation_profile",
                                 "unknown animation profile '" + profile_name + "'");
                        }
                        definition.animation_profile = profile_name;
                        append_profile_animations(definition, profile->second, source_,
                                                  location + ".animation_profile");
                    }
                    if (sprite.contains("animations")) {
                        append_explicit_animations(definition, sprite.at("animations"), source_,
                                                   location + ".animations");
                    }
                    manifest.sprites.push_back(std::move(definition));
                }
            }

            void parse_tilesets(const json& value, AssetManifest& manifest) const {
                require_object(value, source_, "$.tilesets");
                if (value.empty()) {
                    fail(source_, "$.tilesets", "tileset map cannot be empty");
                }
                manifest.tilesets.reserve(value.size());
                for (const auto& [name, tileset] : value.items()) {
                    validate_map_key(name, source_, "$.tilesets");
                    const auto location = "$.tilesets." + name;
                    allow_only(tileset, source_, location,
                               {"description", "texture", "grid", "pivot", "views", "orientations", "animations",
                                "autotiles"});
                    // Materialize the shared entry properties first; later tile rules
                    // can then resolve all targets and variants into this grid's indices.
                    TilesetDefinition definition{
                        .name_space = manifest.name_space,
                        .name = name,
                        .description = description(tileset, source_, location),
                        .texture = entry_texture(tileset, manifest, location),
                        .grid = parse_grid(required(tileset, "grid", source_, location), source_, location + ".grid"),
                        .pivot = tileset.contains("pivot")
                            ? parse_pivot(tileset.at("pivot"), source_, location + ".pivot")
                            : manifest.default_tileset_pivot,
                        .views = {},
                        .orientations = {},
                        .animations = {},
                        .autotiles = {}};
                    if (tileset.contains("views")) {
                        definition.views =
                            parse_views(tileset.at("views"), definition.grid, source_, location + ".views");
                    }
                    if (tileset.contains("orientations")) {
                        definition.orientations = parse_orientations(tileset.at("orientations"), definition.grid,
                                                                     source_, location + ".orientations");
                    }
                    // Resolve target cells and frames now; the map renderer will later choose a
                    // base/autotile cell and substitute an animated frame for targeted cells.
                    if (tileset.contains("animations")) {
                        const auto& animations = tileset.at("animations");
                        require_object(animations, source_, location + ".animations");
                        if (animations.empty()) {
                            fail(source_, location + ".animations", "animation map cannot be empty");
                        }
                        std::unordered_set<CellIndex> animation_targets;
                        for (const auto& [animation_name, animation] : animations.items()) {
                            validate_map_key(animation_name, source_, location + ".animations");
                            const auto animation_location = location + ".animations." + animation_name;
                            allow_only(animation, source_, animation_location,
                                       {"description", "target", "frames", "loop"});
                            const auto& loop = required(animation, "loop", source_, animation_location);
                            if (!loop.is_boolean()) {
                                fail(source_, animation_location + ".loop", "expected a boolean");
                            }
                            TileAnimationDefinition animation_definition{
                                .name = animation_name,
                                .description = description(animation, source_, animation_location),
                                .target = parse_cell(required(animation, "target", source_, animation_location),
                                                     definition.grid, source_, animation_location + ".target"),
                                .frames = parse_frames(required(animation, "frames", source_, animation_location),
                                                       definition.grid, source_, animation_location + ".frames"),
                                .loop = loop.get<bool>()};
                            // Tileset indexes tile animations by target cell, so two named
                            // animations cannot claim the same original cell.
                            if (!animation_targets.emplace(animation_definition.target).second) {
                                fail(source_, animation_location + ".target",
                                     "multiple tile animations target the same cell");
                            }
                            definition.animations.emplace(animation_name, std::move(animation_definition));
                        }
                    }
                    // Autotile rules can choose a base cell at map time; store their
                    // resolved grid cells alongside animations for that later lookup.
                    if (tileset.contains("autotiles")) {
                        definition.autotiles =
                            parse_autotiles(tileset.at("autotiles"), definition.grid, source_, location + ".autotiles");
                    }
                    manifest.tilesets.push_back(std::move(definition));
                }
            }
        };
    }

    AssetManifest ManifestLoader::load(const std::filesystem::path& file) {
        std::ifstream stream(file);
        if (!stream.is_open()) {
            throw Exceptions::runtime_exception(CE_HERE, "Unable to open asset manifest '" + file.string() + "'");
        }
        return parse(stream, file);
    }

    AssetManifest ManifestLoader::parse(std::istream& input, const std::filesystem::path& source) {
        try {
            return Parser(source).parse(json::parse(input));
        }
        catch (const json::exception& error) {
            throw Exceptions::runtime_exception(
                CE_HERE, "Unable to parse asset manifest '" + source.string() + "': " + error.what());
        }
    }
}
