#include <gtest/gtest.h>

#include <assets/manifest.h>
#include <assets/2d/grid-geometry.h>
#include <core/resources/asset-management/manifest-loader.h>
#include <internals/exceptions.h>
#include <math/anchor.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace {
    namespace fs = std::filesystem;
    using namespace CE::Assets;

    fs::path asset_file(const std::string& name) {
        return fs::path(CHERYL_SOURCE_DIR) / "assets" / name;
    }
}

TEST(asset_pivot, supports_arbitrary_normalized_pivots) {
    // Anchor an atlas subrectangle at its bottom center, then inspect its vertex
    // positions, texture coordinates, and repeated corners for two triangles.
    CE::Vertex2D vertices[CE::VAONumbers::vertices_per_quad]{};
    CE::math::Anchor::MakePivot({0.5f, 1.0f}, vertices, 64, 32, 16, 8, 16, 8);

    EXPECT_FLOAT_EQ(vertices[0].x, -8.0f);
    EXPECT_FLOAT_EQ(vertices[0].y, 0.0f);
    EXPECT_FLOAT_EQ(vertices[1].x, 8.0f);
    EXPECT_FLOAT_EQ(vertices[2].y, 8.0f);
    EXPECT_FLOAT_EQ(vertices[0].u, 0.25f);
    EXPECT_FLOAT_EQ(vertices[1].u, 0.5f);
    EXPECT_FLOAT_EQ(vertices[0].v, 0.5f);
    EXPECT_FLOAT_EQ(vertices[2].v, 0.75f);
    EXPECT_EQ(vertices[0].x, vertices[3].x);
    EXPECT_EQ(vertices[2].x, vertices[4].x);
    EXPECT_FLOAT_EQ(vertices[5].x, -8.0f);
    EXPECT_EQ(CE::math::get_pivot(CE::math::BottomCenter), (CE::math::Pivot{0.5f, 1.0f}));

    // An undefined pivot must report a usable argument error with source context.
    try {
        CE::math::Anchor::MakePivot({std::numeric_limits<float>::quiet_NaN(), 0.5f}, vertices, 64, 32, 16, 8);
        FAIL() << "An invalid pivot should throw";
    }
    catch (const CE::Exceptions::invalid_args& error) {
        EXPECT_NE(std::string(error.what()).find("A pivot must be normalized"), std::string::npos);
        EXPECT_NE(std::string(error.what()).find("at line "), std::string::npos);
        EXPECT_NE(std::string(error.what()).find("inside "), std::string::npos);
    }
}

TEST(asset_grid, resolves_spaced_row_major_cells) {
    // Resolve the last cell of a spaced 2-by-3 grid and its occupied bounds.
    const GridDefinition grid{.origin = {2, 3}, .frame = {10, 8}, .spacing = {1, 2}, .rows = 2, .columns = 3};

    EXPECT_EQ(grid.cell_count(), std::size_t{6});
    EXPECT_EQ(grid.cell_index(1, 2), std::size_t{5});
    const auto cell = grid.cell_rect(5);
    EXPECT_EQ(cell.x, 24);
    EXPECT_EQ(cell.y, 13);
    EXPECT_EQ(grid.occupied_right(), 34);
    EXPECT_EQ(grid.occupied_bottom(), 21);

    // The next cell index lies outside the grid.
    EXPECT_THROW(static_cast<void>(grid.cell_rect(6)), CE::Exceptions::bad_request);
}

TEST(asset_grid, builds_vertices_from_dimensions_without_a_gpu_texture) {
    // Build one quad against a known image size and inspect both geometry and UVs.
    const GridDefinition grid{.origin = {4, 2}, .frame = {8, 6}, .rows = 1, .columns = 1};
    const auto geometry = make_grid_geometry(grid, {0.5f, 1.0f}, PixelSize{32, 16});

    ASSERT_EQ(geometry.vertex_count, CE::VAONumbers::vertices_per_quad);
    ASSERT_NE(geometry.vertices, nullptr);
    EXPECT_FLOAT_EQ(geometry.vertices.get()[0].x, -4.0f);
    EXPECT_FLOAT_EQ(geometry.vertices.get()[0].u, 0.125f);
    EXPECT_FLOAT_EQ(geometry.vertices.get()[0].v, 0.5f);

    // Invalid dimensions must fail before creating geometry.
    EXPECT_THROW(make_grid_geometry(grid, {0.5f, 1.0f}, PixelSize{0, 16}), CE::Exceptions::runtime_exception);
    EXPECT_THROW(make_grid_geometry(grid, {0.5f, 1.0f}, PixelSize{8, 16}), CE::Exceptions::runtime_exception);
}

TEST(asset_manifest, parses_every_checked_in_manifest) {
    // Discover the checked-in JSON manifests so each file exercises the parser.
    std::vector<fs::path> manifests;
    for (const auto& entry : fs::directory_iterator(fs::path(CHERYL_SOURCE_DIR) / "assets")) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            manifests.push_back(entry.path());
        }
    }
    std::ranges::sort(manifests);

    // Scope failures to the offending path while checking the expected file set.
    ASSERT_EQ(manifests.size(), std::size_t{7});
    for (const auto& manifest : manifests) {
        SCOPED_TRACE(manifest.string());
        EXPECT_NO_THROW(static_cast<void>(ManifestLoader::load(manifest)));
    }
}

TEST(asset_manifest, expands_profiles_and_inherits_pivots) {
    // Loading the atlas expands its profile definitions into sprites and tilesets.
    const auto manifest = ManifestLoader::load(asset_file("atlas.json"));
    ASSERT_EQ(manifest.sprites.size(), std::size_t{101});
    ASSERT_EQ(manifest.tilesets.size(), std::size_t{35});

    // Inspect one generated sprite for its full name, inherited pivot, and animations.
    const auto sprite = std::ranges::find_if(manifest.sprites,
                                             [](const auto& value) { return value.name == "soldier_swordsman_cyan"; });
    ASSERT_NE(sprite, manifest.sprites.end());
    EXPECT_EQ(sprite->id(), "miniworld:soldier_swordsman_cyan");
    EXPECT_EQ(sprite->pivot, (CE::math::Pivot{0.5f, 1.0f}));
    ASSERT_EQ(sprite->animations.size(), std::size_t{20});

    // Follow its south-facing walk cycle through cell selection, timing, and looping.
    const auto walk_south = std::ranges::find_if(sprite->animations, [](const auto& animation) {
        return animation.name == "walk" && animation.facing == "south";
    });
    ASSERT_NE(walk_south, sprite->animations.end());
    ASSERT_EQ(walk_south->frames.size(), std::size_t{4});
    EXPECT_EQ(walk_south->frames[0].cell, std::size_t{1});
    EXPECT_EQ(walk_south->frames[3].cell, std::size_t{4});
    EXPECT_EQ(walk_south->frames[0].duration, std::chrono::milliseconds(200));
    EXPECT_TRUE(walk_south->loop);
}

TEST(asset_manifest, retains_tile_animations_views_and_wang_autotiles) {
    // Load the overworld tileset and verify its derived views, animations, and autotiles.
    const auto manifest = ManifestLoader::load(asset_file("punyworld-overworld.json"));
    ASSERT_EQ(manifest.tilesets.size(), std::size_t{1});
    const auto& tileset = manifest.tilesets.front();

    EXPECT_EQ(tileset.id(), "punyworld:overworld");
    EXPECT_EQ(tileset.grid.cell_count(), std::size_t{1755});
    EXPECT_EQ(tileset.views.size(), std::size_t{7});
    EXPECT_EQ(tileset.animations.size(), std::size_t{70});
    EXPECT_EQ(tileset.autotiles.size(), std::size_t{2});
    EXPECT_EQ(tileset.animations.at("tile-270").frames.size(), std::size_t{4});

    // Check corner-based terrain and compare its total variant entries to its tile count.
    const auto& terrain = std::get<WangAutotileDefinition>(tileset.autotiles.at("overworld"));
    EXPECT_EQ(terrain.type, WangType::Corner);
    EXPECT_EQ(terrain.terrains.size(), std::size_t{12});
    EXPECT_EQ(terrain.tiles.size(), std::size_t{168});
    EXPECT_FALSE(terrain.variants.empty());
    const auto terrain_variant_count =
        std::accumulate(terrain.variants.begin(), terrain.variants.end(), std::size_t{},
                        [](const std::size_t count, const auto& entry) { return count + entry.second.size(); });
    EXPECT_EQ(terrain_variant_count, terrain.tiles.size());

    // Check the second autotile uses edge-based terrain with its own tile set.
    const auto& pathways = std::get<WangAutotileDefinition>(tileset.autotiles.at("pathways"));
    EXPECT_EQ(pathways.type, WangType::Edge);
    EXPECT_EQ(pathways.terrains.size(), std::size_t{3});
    EXPECT_EQ(pathways.tiles.size(), std::size_t{45});
    EXPECT_FALSE(pathways.variants.empty());
}

TEST(asset_manifest, parses_explicit_animations_orientations_and_bitmasks) {
    // Define a minimal manifest with a sprite grid, explicit animation frames,
    // and a tileset whose neighbor bitmasks map to cells.
    std::istringstream input(R"json({
      "$schema": "./schemas/asset-manifest-1.0.schema.json",
      "version": "1.0",
      "namespace": "test",
      "texture": "sheet.png",
      "defaults": {
        "sprite": {"pivot": {"x": 0.5, "y": 1}},
        "tileset": {"pivot": {"x": 0.5, "y": 0.5}}
      },
      "sprites": {
        "actor": {
          "grid": {
            "origin": {"x": 0, "y": 0},
            "frame": {"width": 16, "height": 16},
            "spacing": {"x": 1, "y": 2},
            "rows": 2,
            "columns": 2,
            "cell_order": "row-major"
          },
          "views": {"top": {"row": 0, "column": 0, "rows": 1, "columns": 2}},
          "orientations": {"east": {"row": 0, "column": 1}},
          "animations": {
            "blink": {
              "frames": [
                {"cell": {"index": 0}, "duration_ms": 80},
                {"cell": {"row": 1, "column": 1}, "duration_ms": 120}
              ],
              "loop": false
            }
          }
        }
      },
      "tilesets": {
        "terrain": {
          "grid": {
            "origin": {"x": 0, "y": 0},
            "frame": {"width": 16, "height": 16},
            "spacing": {"x": 0, "y": 0},
            "rows": 1,
            "columns": 2,
            "cell_order": "row-major"
          },
          "autotiles": {
            "edges": {
              "type": "four-neighbor",
              "bit_order": ["north", "east", "south", "west"],
              "cases": {"0": {"index": 0}, "15": {"index": 1}}
            }
          }
        }
      }
    })json");

    // Resolve paths relative to the manifest and cell coordinates within the grid.
    const auto manifest = ManifestLoader::parse(input, "/tmp/assets/test.json");
    ASSERT_EQ(manifest.sprites.size(), std::size_t{1});
    const auto& sprite = manifest.sprites.front();
    EXPECT_EQ(sprite.texture, fs::path("/tmp/assets/sheet.png"));
    EXPECT_EQ(sprite.orientations.at("east"), std::size_t{1});
    ASSERT_EQ(sprite.animations.size(), std::size_t{1});
    EXPECT_EQ(sprite.animations.front().frames[1].cell, std::size_t{3});
    EXPECT_EQ(sprite.animations.front().frames[1].duration, std::chrono::milliseconds(120));

    // Check the number of bit directions and the full-neighbor mask's cell mapping.
    const auto& bitmask = std::get<BitmaskAutotileDefinition>(manifest.tilesets.front().autotiles.at("edges"));
    EXPECT_EQ(bitmask.bit_order.size(), std::size_t{4});
    EXPECT_EQ(bitmask.cases.at(15), std::size_t{1});
}

TEST(asset_manifest, rejects_out_of_range_cells) {
    // Declare a one-cell grid but point the east orientation at cell index one.
    std::istringstream input(R"json({
      "$schema": "./schemas/asset-manifest-1.0.schema.json",
      "version": "1.0",
      "namespace": "bad",
      "texture": "sheet.png",
      "defaults": {
        "sprite": {"pivot": {"x": 0.5, "y": 0.5}},
        "tileset": {"pivot": {"x": 0.5, "y": 0.5}}
      },
      "sprites": {
        "actor": {
          "grid": {
            "origin": {"x": 0, "y": 0},
            "frame": {"width": 16, "height": 16},
            "spacing": {"x": 0, "y": 0},
            "rows": 1,
            "columns": 1,
            "cell_order": "row-major"
          },
          "orientations": {"east": {"index": 1}}
        }
      }
    })json");

    // Parsing must reject the reference and identify its manifest in the error.
    try {
        static_cast<void>(ManifestLoader::parse(input, "bad.json"));
        FAIL() << "An out-of-range cell should throw";
    }
    catch (const CE::Exceptions::runtime_exception& error) {
        EXPECT_NE(std::string(error.what()).find("bad.json"), std::string::npos);
        EXPECT_NE(std::string(error.what()).find("at line "), std::string::npos);
        EXPECT_NE(std::string(error.what()).find("inside "), std::string::npos);
    }
}
