#include <cheryl/core/resources/map-storage/cell-patch.h>
#include <cheryl/maps/terrain-map.h>
#include "world-document.h"
#include "tilemap-editor.h"
#include "cave.h"
#include "variable-documents.h"
#include "generated-expectations.h"

#include <internals/exceptions.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

#if defined(CE_ENGINE_STATIC) || defined(CE_BUILDING_ENGINE)
#error This consumer must use the normal shared Engine import contract.
#endif

namespace {
    using namespace WorldProof;
    namespace Patch = Storage::CellPatches;
    using CE::Exceptions::bad_request;

    void require(const bool condition, const std::string& detail) {
        if (!condition)
            throw std::runtime_error(detail);
    }
    template <class E, class F, class Check> void refuses(F&& operation, Check&& check) {
        bool refused = false;
        try {
            operation();
        } catch (const E& error) {
            check(error);
            std::cout << "Expected refusal: " << error.what() << '\n';
            refused = true;
        }
        require(refused, "Expected operation to refuse");
    }
    const std::array keys{KeyId{"proof:earth", {101}}, KeyId{"proof:stone", {307}},   KeyId{"proof:water", {613}},
                          KeyId{"proof:flag", {911}},  KeyId{"proof:flower", {1201}}, KeyId{"proof:wall", {2017}},
                          KeyId{"proof:floor", {4051}}};
    const std::array semantics{SemanticKey{"proof:wall", {11}}, SemanticKey{"proof:floor", {23}}};

    Storage::Snapshot literal_base() {
        return {
            "proof:literal",
            3,
            2,
            {{"ground", {"proof:earth", "proof:stone", std::nullopt, "proof:water", "proof:earth", "proof:stone"}},
             {"detail", {std::nullopt, "proof:flag", std::nullopt, std::nullopt, std::nullopt, "proof:flower"}}},
            {{"entry", "ground", 0, 1}, {"lookout", "detail", 2, 1}}
        };
    }
    Storage::Snapshot literal_target() {
        // Independent full literal, never produced by the bridge/diff/apply.
        return {
            "proof:literal",
            3,
            2,
            {{"ground", {"proof:water", "proof:stone", "proof:earth", "proof:water", std::nullopt, "proof:stone"}},
             {"detail", {"proof:flag", std::nullopt, std::nullopt, std::nullopt, "proof:flower", "proof:flower"}}},
            {{"entry", "ground", 0, 1}, {"lookout", "detail", 2, 1}}
        };
    }
    constexpr std::array<std::uint32_t, 12> literal_base_ids{101, 307, 0, 613, 101, 307, 0, 911, 0, 0, 0, 1201};
    constexpr std::array<std::uint32_t, 12> literal_target_ids{613, 307, 101, 613, 0, 307, 911, 0, 0, 0, 1201, 1201};
    void check_map(const Tiles::Map& map, const Tiles::Extent size, const std::span<const std::uint32_t> ids) {
        require(map.extent() == size && map.layer_count() == 2, "Numeric map dimensions/layer count changed");
        require(ids.size() == std::size_t(size.columns) * size.rows * 2, "Invalid independent expected map");
        std::size_t index = 0;
        for (std::int64_t layer = 0; layer < 2; ++layer)
            for (std::uint32_t row = 0; row < size.rows; ++row)
                for (std::uint32_t column = 0; column < size.columns; ++column)
                    require(map.get({layer}, {column, row}).value == ids[index++], "Numeric cell differs from literal expected value");
    }
    void write_raw(const std::filesystem::path& path, const std::string_view bytes) {
        std::ofstream out(path, std::ios::binary);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.close();
        require(!out.fail(), "Could not write literal input fixture");
    }
    std::string bytes(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        require(input.is_open(), "Could not inspect fixture bytes");
        std::ostringstream out;
        out << input.rdbuf();
        require(!input.bad() && !out.fail(), "Could not read complete fixture bytes");
        return out.str();
    }
    constexpr std::string_view literal_document = R"({
  "format":"cheryl.map-fixture", "version":1, "id":"proof:literal", "width":3, "height":2,
  "layers":[
    {"id":"ground","cells":["proof:earth","proof:stone",null,"proof:water","proof:earth","proof:stone"]},
    {"id":"detail","cells":[null,"proof:flag",null,null,null,"proof:flower"]}
  ],
  "spawns":[{"name":"entry","layer":"ground","x":0,"y":1},
            {"name":"lookout","layer":"detail","x":2,"y":1}]
})";

    void literal_author(const std::filesystem::path& path) {
        const KeyTable table(keys);
        for (const auto& entry : keys) {
            require(table.key(entry.id) == entry.key, "ID -> key mapping differs");
            require(table.id(entry.key) == entry.id, "Key -> ID mapping differs");
        }
        require(table.id(std::nullopt).value == 0 && !table.key({0}), "Null/zero is not reversible");
        const auto input = path.string() + ".input";
        write_raw(input, literal_document);
        const auto base = Storage::load(input);
        require(base == literal_base(), "Literal parse changed metadata or cells");
        auto live = LiteralOwner::load(input, table);
        require(live->snapshot() == literal_base(), "Metadata/key round trip changed the base");
        check_map(live->map(), {3, 2}, literal_base_ids);
        auto other = LiteralOwner::from(base, table);
        const auto old = live->map();
        live->map().set({0}, {0, 0}, {613});
        live->map().set({0}, {2, 0}, {101});
        live->map().set({0}, {1, 1}, {0});
        live->map().set({1}, {0, 0}, {911});
        live->map().set({1}, {1, 0}, {0});
        live->map().set({1}, {1, 1}, {1201});
        auto target = live->snapshot();
        require(target == literal_target(), "Complete target does not match independent literal fields");
        check_map(live->map(), {3, 2}, literal_target_ids);
        const auto patch = Patch::diff(base, target);
        const std::vector<Patch::Edit> edits{{"ground", 0, 0, "proof:water"}, {"ground", 2, 0, "proof:earth"},
                                             {"ground", 1, 1, std::nullopt},  {"detail", 0, 0, "proof:flag"},
                                             {"detail", 1, 0, std::nullopt},  {"detail", 1, 1, "proof:flower"}};
        require(patch.edits == edits, "Final snapshot diff is not ordered/complete");
        const auto applied = Patch::apply(base, patch);
        require(applied == literal_target() && base == literal_base(), "Apply changed result/base");
        check_map(old, {3, 2}, literal_base_ids);
        check_map(other->map(), {3, 2}, literal_base_ids);
        live.reset();
        other.reset();
        target.layers.clear();
        Storage::save(path, applied); // File publication is a separate checked operation.
        require(bytes(input) == literal_document, "Untouched literal source file changed");
        std::cout << "Literal load/edit/full diff/apply/checked save: 12 cells, two layers/spawns; PASS\n";
    }
    void literal_verify(const std::filesystem::path& path, const bool base) {
        const auto expected = base ? literal_base() : literal_target();
        const auto loaded = Storage::load(path);
        require(loaded == expected, "Fresh reload differs in at least one snapshot field");
        const auto live = LiteralOwner::load(path, KeyTable(keys));
        require(live->snapshot() == expected, "Fresh metadata/keys were not retained");
        check_map(live->map(), {3, 2}, base ? literal_base_ids : literal_target_ids);
        std::cout << "FRESH process: complete independent literal snapshot + 12 numeric cells PASS\n";
    }

    Storage::Snapshot cave_base() {
        return {
            "proof:cave",
            5,
            4,
            {{"terrain", {"proof:wall",  "proof:wall",  "proof:wall", "proof:wall", "proof:wall",  "proof:wall",  "proof:floor",
                          "proof:floor", "proof:floor", "proof:wall", "proof:wall", "proof:floor", "proof:floor", "proof:floor",
                          "proof:wall",  "proof:wall",  "proof:wall", "proof:wall", "proof:wall",  "proof:wall"}},
             {"marks", {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                        std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                        std::nullopt, std::nullopt, std::nullopt, std::nullopt, "proof:flag", std::nullopt}}},
            {{"entry", "terrain", 1, 1}, {"beacon", "marks", 3, 3}}
        };
    }
    Storage::Snapshot cave_target() {
        return {
            "proof:cave",
            5,
            4,
            {{"terrain", {"proof:wall", "proof:wall",  "proof:wall", "proof:wall", "proof:wall",  "proof:wall",  "proof:floor",
                          "proof:wall", "proof:floor", "proof:wall", "proof:wall", "proof:floor", "proof:floor", "proof:floor",
                          "proof:wall", "proof:wall",  "proof:wall", "proof:wall", "proof:wall",  "proof:wall"}},
             {"marks", {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                        std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                        std::nullopt, std::nullopt, std::nullopt, std::nullopt, "proof:flag", std::nullopt}}},
            {{"entry", "terrain", 1, 1}, {"beacon", "marks", 3, 3}}
        };
    }
    constexpr std::array<std::uint8_t, 20> cave_occupancy{1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1};
    constexpr std::array<std::uint32_t, 20> cave_target_labels{11, 11, 11, 11, 11, 11, 23, 11, 23, 11,
                                                               11, 23, 23, 23, 11, 11, 11, 11, 11, 11};
    constexpr std::array<std::uint32_t, 40> cave_target_ids{2017, 2017, 2017, 2017, 2017, 2017, 4051, 2017, 4051, 2017,
                                                            2017, 4051, 4051, 4051, 2017, 2017, 2017, 2017, 2017, 2017,
                                                            0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
                                                            0,    0,    0,    0,    0,    0,    0,    0,    911,  0};
    CE::Assets::Tilesets::BitmaskRule rule() {
        namespace Atlas = CE::Assets::Tilesets;
        Atlas::BitmaskRule result;
        result.type = Atlas::BitmaskType::FourNeighbor;
        result.bit_order = {Atlas::Direction::North, Atlas::Direction::East, Atlas::Direction::South, Atlas::Direction::West};
        for (std::uint32_t mask = 0; mask < 16; ++mask)
            result.cases.emplace(mask, mask); // Explicit mask -> original selection target domain.
        return result;
    }
    std::vector<Tiles::TileId> targets() {
        return {{1001}, {1002}, {1003}, {1004}, {1005}, {1006}, {1007}, {1008},
                {1009}, {1010}, {1011}, {1012}, {1013}, {1014}, {1015}, {1016}};
    }
    // Independent full-grid reference uses only literal label equality/cardinal
    // adjacency, not Selection or the incremental edit/bridge implementation.
    void check_terrain(const TerrainOwner& owner, const std::span<const std::uint32_t> labels) {
        const auto& terrain = owner.terrain();
        require(labels.size() == 20, "Bad independent label fixture");
        for (std::int64_t row = 0; row < 4; ++row)
            for (std::int64_t column = 0; column < 5; ++column) {
                const auto index = static_cast<std::size_t>(row * 5 + column);
                const auto label = labels[index];
                require(terrain.label({column, row}).value == label, "Semantic meaning changed");
                const auto same = [&](const std::int64_t x, const std::int64_t y) {
                    return x >= 0 && x < 5 && y >= 0 && y < 4 && labels[static_cast<std::size_t>(y * 5 + x)] == label;
                };
                const unsigned mask = (same(column, row - 1) ? 1U : 0U) + (same(column + 1, row) ? 2U : 0U) +
                                      (same(column, row + 1) ? 4U : 0U) + (same(column - 1, row) ? 8U : 0U);
                require(terrain.tiles().get({0}, {column, row}).value == (label ? 1001 + mask : 0), "Derived visual differs from oracle");
                require(terrain.tiles().get({1}, {column, row}).value == (index == 18 ? 911U : 0U), "Other layer changed");
            }
    }
    void generated_author(const std::filesystem::path& path) {
        const std::array<ProcGen::Index, 2> extent{5, 4};
        // Real seeded initial fill, then a declared cellular rule clears every
        // interior cell. This yields a literal independent full output oracle.
        const auto cave = ProcGen::generate_cave(
            extent, {42, 0x776f726c64, 7},
            {.solid_fill = 4500, .steps = 1, .birth = 0, .survival = 0, .connectivity = ProcGen::CaveConnectivity::retain_components},
            {64, 8192, 10000}
        );
        require(cave.has_value(), "Actual cave producer refused admitted 2D request");
        require(
            cave->domain().dimensions() == 2 && cave->domain().extents()[0] == 5 && cave->domain().extents()[1] == 4,
            "Cave dimensions not 5x4"
        );
        require(std::ranges::equal(cave->occupancy(), cave_occupancy), "Actual occupancy differs from literal perimeter/interior");
        require(
            cave->measurements().floor == 6 && cave->measurements().solid == 14 && cave->usage().random_draws > 0,
            "Real seeded cave fill/smoothing did not execute"
        );
        Storage::Snapshot source{
            "proof:cave",
            5,
            4,
            {{"terrain", {}}, {"marks", std::vector<std::optional<std::string>>(20)}},
            {{"entry", "terrain", 1, 1}, {"beacon", "marks", 3, 3}}
        };
        for (const auto occupied : cave->occupancy()) {
            require(occupied <= 1, "Unknown occupancy meaning");
            source.layers[0].cells.push_back(occupied ? "proof:wall" : "proof:floor");
        }
        source.layers[1].cells[18] = "proof:flag";
        require(source == cave_base(), "Generated semantic snapshot differs from complete literal");
        auto live = TerrainOwner::from(source, KeyTable(keys), {0}, semantics, rule(), targets());
        auto sibling = TerrainOwner::from(source, KeyTable(keys), {0}, semantics, rule(), targets());
        std::array<std::uint32_t, 20> initial_labels{};
        for (std::size_t i = 0; i < initial_labels.size(); ++i)
            initial_labels[i] = cave_occupancy[i] ? 11 : 23;
        check_terrain(*live, initial_labels);
        const auto old_tiles = live->terrain().tiles();
        const auto old_snapshot = live->snapshot();
        const std::array edits{Tiles::TerrainEdit{{2, 1}, {11}}, Tiles::TerrainEdit{{2, 1}, {23}}, Tiles::TerrainEdit{{2, 1}, {11}}};
        const auto affected = live->edit_batch(edits, {3, 5, 100});
        require(affected == std::vector<Tiles::Point>{{2, 1}, {2, 0}, {3, 1}, {2, 2}, {1, 1}}, "Cardinal invalidation/order differs");
        check_terrain(*live, cave_target_labels);
        check_terrain(*sibling, initial_labels);
        require(old_snapshot == cave_base() && sibling->snapshot() == cave_base(), "Old result/sibling changed");
        for (std::int64_t y = 0; y < 4; ++y)
            for (std::int64_t x = 0; x < 5; ++x)
                for (std::int64_t layer = 0; layer < 2; ++layer)
                    require(old_tiles.get({layer}, {x, y}) == sibling->terrain().tiles().get({layer}, {x, y}), "Old copied tiles changed");
        const auto target = live->snapshot();
        require(target == cave_target(), "Persisted derived visuals instead of complete semantic meaning");
        const auto patch = Patch::diff(source, target); // Coalesce FINAL snapshot, not duplicate brush samples.
        require(patch.edits == std::vector<Patch::Edit>{{"terrain", 2, 1, "proof:wall"}}, "Final semantic diff is not one unique address");
        const auto applied = Patch::apply(source, patch);
        require(applied == cave_target(), "Generated final patch differs from independent literal");
        live.reset();
        sibling.reset();
        source.layers.clear();
        Storage::save(path, applied);
        std::cout << "Actual 2D cave -> semantic keys/labels -> cardinal visual IDs -> final diff/save PASS; work=" << cave->usage().work
                  << ", draws=" << cave->usage().random_draws << '\n';
    }
    void generated_verify(const std::filesystem::path& path) {
        const auto loaded = Storage::load(path);
        require(loaded == cave_target(), "Fresh generated snapshot changed full literal meaning/metadata");
        const auto numeric = LiteralOwner::from(loaded, KeyTable(keys));
        check_map(numeric->map(), {5, 4}, cave_target_ids);
        const auto terrain = TerrainOwner::from(loaded, KeyTable(keys), {0}, semantics, rule(), targets());
        check_terrain(*terrain, cave_target_labels);
        require(terrain->snapshot() == cave_target(), "Fresh semantic reconstruction changed any snapshot field");
        std::cout << "FRESH process: 20 semantic labels/derived visuals + 40 numeric meaning cells PASS; no generator replay used\n";
    }

    Storage::Snapshot editor_expected(const bool east) {
        // Complete literal expectations, independent of editor/document conversion.
        if (east)
            return {
                "tilemap:east",
                3,
                2,
                {{"ground", {"tilemap:grey", "tilemap:brown", std::nullopt, "tilemap:black", "tilemap:red", "tilemap:grey"}},
                 {"detail", {"tilemap:red", std::nullopt, "tilemap:red", std::nullopt, "tilemap:black", std::nullopt}}},
                {{"entry", "ground", 0, 0}, {"marker", "detail", 2, 0}}
            };
        return {
            "tilemap:west",
            3,
            2,
            {{"ground", {"tilemap:brown", "tilemap:black", "tilemap:grey", "tilemap:red", "tilemap:red", "tilemap:brown"}},
             {"detail", {std::nullopt, "tilemap:red", std::nullopt, "tilemap:grey", std::nullopt, std::nullopt}}},
            {{"entry", "ground", 0, 0}, {"marker", "detail", 1, 0}}
        };
    }
    namespace Events = CE::Input::Events;
    using Key = CE::Input::KeyboardKey;
    Events::Record press(const Key key) {
        return {
            0,
            {},
            1,
            CE::Input::DeviceKind::Keyboard,
            Events::Button{CE::Input::control_id(key), Events::ButtonPhase::Press, CE::Input::Modifiers::None, -1, -1, key}
        };
    }
    Events::Record paint(const double x, const double y) {
        const auto button = CE::Input::MouseButton::Left;
        return {
            0,
            {},
            2,
            CE::Input::DeviceKind::Mouse,
            Events::Button{
                CE::Input::control_id(button), Events::ButtonPhase::Press, CE::Input::Modifiers::None, -1, -1, Key::Unknown, button,
                Events::Pointer{x, y}
            }
        };
    }
    void editor_events(DemoTilemapEditor& editor, const std::initializer_list<Events::Record> events) {
        editor.consume({events.begin(), events.size()}, {{400, 200}, {800, 600}});
    }
    void editor_author(const std::filesystem::path& directory) {
        DemoTilemapEditor editor(directory);
        require(
            !std::filesystem::exists(directory / "west.json") && !std::filesystem::exists(directory / "east.json"),
            "Editor performed automatic startup file I/O"
        );
        editor_events(editor, {press(Key::Digit6), paint(72, 144), press(Key::F8)});
        require(Storage::load(directory / "west.json") == editor_expected(false), "Actual F8 west output differs from literal");
        editor_events(editor, {press(Key::Digit3), paint(72, 144), press(Key::F9)});
        require(editor.document(0).snapshot() == editor_expected(false), "F9 failed to restore west cells/metadata");
        editor_events(editor, {press(Key::Digit2), press(Key::T), press(Key::Digit4), paint(212, 156), press(Key::F8)});
        require(Storage::load(directory / "east.json") == editor_expected(true), "Actual selected-layer F8 east output differs");
        require(editor.document(0).snapshot() == editor_expected(false), "Saving east changed west");
        std::cout << "Real private editor ordered paint/F8/change/F9: two independent full literal documents PASS\n";
    }
    void editor_verify(const std::filesystem::path& directory) {
        require(Storage::load(directory / "west.json") == editor_expected(false), "Fresh complete west snapshot differs");
        require(Storage::load(directory / "east.json") == editor_expected(true), "Fresh complete east snapshot differs");
        DemoTilemapEditor editor(directory);
        editor_events(editor, {press(Key::F9), press(Key::Digit2), press(Key::F9)});
        constexpr std::array<std::uint32_t, 12> west{7, 8, 9, 10, 10, 7, 0, 10, 0, 9, 0, 0};
        constexpr std::array<std::uint32_t, 12> east{9, 7, 0, 8, 10, 9, 10, 0, 10, 0, 8, 0};
        check_map(editor.map(0), {3, 2}, west);
        check_map(editor.map(1), {3, 2}, east);
        require(
            editor.document(0).snapshot() == editor_expected(false) && editor.document(1).snapshot() == editor_expected(true),
            "Fresh actual editor adoption lost complete literal metadata"
        );
        std::cout << "FRESH process: full west/east literal fields +24 independent numeric cells through F9 PASS\n";
    }

    Storage::Snapshot semantic_expected(const bool east) {
        // Full independent literals: no conversion/authoring helper produces these.
        if (east)
            return {
                "terrain:east-authored",
                3,
                2,
                {{"soil", {"terrain:four", std::nullopt, "terrain:one", "terrain:one", "terrain:two", std::nullopt}},
                 {"accent", {std::nullopt, "terrain:three", std::nullopt, "terrain:three", std::nullopt, "terrain:two"}}},
                {{"arrival", "soil", 2, 0}, {"scenery", "accent", 1, 1}, {"exit", "soil", 0, 1}}
            };
        return {
            "terrain:west-authored",
            3,
            2,
            {{"soil", {"terrain:one", "terrain:one", "terrain:two", std::nullopt, "terrain:three", "terrain:three"}},
             {"accent", {"terrain:two", std::nullopt, "terrain:two", "terrain:four", std::nullopt, std::nullopt}}},
            {{"arrival", "soil", 1, 0}, {"scenery", "accent", 0, 1}, {"exit", "soil", 2, 1}}
        };
    }
    constexpr std::array<std::array<std::uint32_t, 12>, 2> semantic_label_values{
        {{1, 1, 2, 0, 3, 3, 2, 0, 2, 4, 0, 0}, {4, 0, 1, 1, 2, 0, 0, 3, 0, 3, 0, 2}}
    };
    constexpr std::array<std::array<std::uint32_t, 12>, 2> semantic_meaning_values{
        {{101, 101, 203, 0, 307, 307, 203, 0, 203, 409, 0, 0}, {409, 0, 101, 101, 203, 0, 0, 307, 0, 307, 0, 203}}
    };
    void check_semantic_editor(const DemoTilemapEditor& editor, const bool edited_east = false) {
        const std::array entries{
            KeyId{"terrain:one", {101}}, KeyId{"terrain:two", {203}}, KeyId{"terrain:three", {307}}, KeyId{"terrain:four", {409}}
        };
        const KeyTable meaning(entries);
        for (std::size_t instance = 0; instance < 2; ++instance) {
            auto expected = semantic_expected(instance == 1);
            auto label_values = semantic_label_values[instance];
            auto meaning_values = semantic_meaning_values[instance];
            if (edited_east && instance == 1) {
                expected.layers[1].cells[4] = "terrain:two";
                label_values[10] = 2;
                meaning_values[10] = 203;
            }
            const auto snapshot = editor.terrain_document(instance).snapshot();
            require(snapshot == expected, "Complete semantic cells/metadata differ from literal");
            const auto numeric = LiteralOwner::from(snapshot, meaning);
            check_map(numeric->map(), {3, 2}, meaning_values);
            for (std::int64_t layer = 0; layer < 2; ++layer)
                for (std::int64_t y = 0; y < 2; ++y)
                    for (std::int64_t x = 0; x < 3; ++x) {
                        const auto offset = static_cast<std::size_t>(layer * 6 + y * 3 + x);
                        const auto label = label_values[offset];
                        const auto& terrain = editor.terrain(instance, {layer});
                        require(terrain.label({x, y}).value == label, "Rebuilt opaque semantic label differs");
                        // Independent cardinal oracle; never calls selection/batch or cached visuals.
                        const auto same = [&](const std::int64_t nx, const std::int64_t ny) {
                            return nx >= 0 && nx < 3 && ny >= 0 && ny < 2 &&
                                   label_values[static_cast<std::size_t>(layer * 6 + ny * 3 + nx)] == label;
                        };
                        const unsigned mask = (same(x, y - 1) ? 1u : 0u) | (same(x + 1, y) ? 2u : 0u) | (same(x, y + 1) ? 4u : 0u) |
                                              (same(x - 1, y) ? 8u : 0u);
                        const auto visual = label ? 7 + mask % 4 : 0;
                        require(terrain.tiles().get({layer}, {x, y}).value == visual, "Rebuilt visual differs from independent oracle");
                    }
        }
    }
    void semantic_author(const std::filesystem::path& directory) {
        // Seed only metadata with empty cells; all final semantic cells are painted
        // through actual admitted per-event editor records, not adapter mutation.
        for (const bool east : {false, true}) {
            auto seed = semantic_expected(east);
            for (auto& layer : seed.layers)
                for (auto& cell : layer.cells)
                    cell.reset();
            Storage::save(directory / (east ? "east-terrain.json" : "west-terrain.json"), seed);
        }
        DemoTilemapEditor editor(directory);
        editor_events(editor, {press(Key::F8)}); // Independent literal file must survive all semantic actions.
        const auto literal = bytes(directory / "west.json");
        editor_events(editor, {press(Key::B)});
        for (const bool east : {false, true}) {
            editor_events(editor, {press(east ? Key::Digit2 : Key::Digit1), press(Key::F9)});
            if (east) {
                editor_events(
                    editor, {press(Key::G), press(Key::Digit6), paint(180, 140), press(Key::Digit3), paint(244, 140), paint(180, 156),
                             press(Key::Digit4), paint(212, 156), press(Key::T), press(Key::Digit5), paint(212, 140), paint(180, 156),
                             press(Key::Digit4), paint(244, 156), press(Key::F8)}
                );
                editor_events(editor, {paint(212, 156), press(Key::F9)});
            } else {
                editor_events(
                    editor, {press(Key::G), press(Key::Digit3), paint(40, 128), paint(72, 128), press(Key::Digit4), paint(104, 128),
                             press(Key::Digit5), paint(72, 144), paint(104, 144), press(Key::T), press(Key::Digit4), paint(40, 128),
                             paint(104, 128), press(Key::Digit6), paint(40, 144), press(Key::F8)}
                );
                editor_events(editor, {paint(72, 144), press(Key::F9)});
            }
            require(
                Storage::load(directory / (east ? "east-terrain.json" : "west-terrain.json")) == semantic_expected(east),
                "Paint/save/change/load differs from independent semantic literal"
            );
        }
        check_semantic_editor(editor);
        require(
            bytes(directory / "west.json") == literal && !std::filesystem::exists(directory / "east.json"),
            "Semantic action changed literal files"
        );
        std::cout << "Actual editor semantic paint both layers/F8/change/F9, distinct aliased labels, independent modes/docs PASS\n";
    }
    void semantic_verify(const std::filesystem::path& directory) {
        for (const bool east : {false, true})
            require(
                Storage::load(directory / (east ? "east-terrain.json" : "west-terrain.json")) == semantic_expected(east),
                "Fresh full semantic snapshot fields differ"
            );
        DemoTilemapEditor editor(directory);
        editor_events(editor, {press(Key::B), press(Key::F9), press(Key::Digit2), press(Key::F9)});
        check_semantic_editor(editor);
        std::cout << "FRESH process F9: all24 labels/numeric meaning IDs/derived visuals and every ordered metadata field PASS\n";
    }

    void history_author(const std::filesystem::path& directory) {
        semantic_author(directory); // Proven actual event setup; no test-only owner replacement.
        DemoTilemapEditor editor(directory);
        editor_events(editor, {press(Key::F9), press(Key::Digit6), paint(72, 144), press(Key::F6)});
        require(editor.document(0).snapshot().layers[0].cells[4] == std::nullopt, "Literal undo failed");
        editor_events(editor, {press(Key::F7), press(Key::F8)});
        require(Storage::load(directory / "west.json") == editor_expected(false), "Literal redo/save differs from full literal");
        editor_events(editor, {press(Key::Digit3), paint(72, 144), press(Key::F9)});
        require(
            editor.document(0).snapshot() == editor_expected(false) && editor.history(0, DemoTilemapEditor::Mode::Literal).undo == 0,
            "Literal LOAD did not reset only its history"
        );
        editor_events(
            editor, {press(Key::Digit2), press(Key::T), press(Key::Digit4), paint(212, 156), press(Key::F6), press(Key::F7), press(Key::F8)}
        );
        require(Storage::load(directory / "east.json") == editor_expected(true), "East literal undo/redo/save differs");
        editor_events(editor, {press(Key::B), press(Key::Digit1), press(Key::G), press(Key::F9), press(Key::Digit6), paint(40, 144)});
        auto changed = semantic_expected(false);
        changed.layers[0].cells[3] = "terrain:four";
        require(editor.terrain_document(0).snapshot() == changed, "Semantic change lost metadata/meaning");
        editor_events(editor, {press(Key::F6)});
        require(editor.terrain_document(0).snapshot() == semantic_expected(false), "Semantic undo lost full original fields");
        editor_events(editor, {press(Key::F7)});
        require(editor.terrain_document(0).snapshot() == changed, "Semantic redo lost full changed fields");
        editor_events(editor, {press(Key::F6), press(Key::F8)}); // Persist UNDONE west state.
        require(Storage::load(directory / "west-terrain.json") == semantic_expected(false), "Saved derived rather than undone meaning");
        editor_events(
            editor, {press(Key::Digit2), press(Key::F9), press(Key::T), press(Key::Digit4), paint(212, 156), press(Key::F6), press(Key::F7),
                     press(Key::F8)}
        );
        auto east = semantic_expected(true);
        east.layers[1].cells[4] = "terrain:two";
        require(Storage::load(directory / "east-terrain.json") == east, "Saved REDONE semantic state differs");
        check_semantic_editor(editor, true);
        editor_events(editor, {press(Key::Digit6), paint(212, 156), press(Key::F9)});
        require(
            editor.terrain_document(1).snapshot() == east && editor.history(1, DemoTilemapEditor::Mode::Terrain).undo == 0,
            "Semantic reload lost full saved meaning or history reset"
        );
        require(editor.history(0, DemoTilemapEditor::Mode::Terrain).redo == 1, "East LOAD cleared independent west redo");
        std::cout << "Actual literal/semantic edit/F6/F7/F8/F9; saved undone west and redone east meaning PASS\n";
    }
    void history_verify(const std::filesystem::path& directory) {
        editor_verify(directory); // Independent complete literal expectations +24 numeric cells.
        require(Storage::load(directory / "west-terrain.json") == semantic_expected(false), "Fresh undone west snapshot differs");
        auto east = semantic_expected(true);
        east.layers[1].cells[4] = "terrain:two";
        require(Storage::load(directory / "east-terrain.json") == east, "Fresh redone east snapshot differs");
        DemoTilemapEditor editor(directory);
        editor_events(editor, {press(Key::B), press(Key::F9), press(Key::Digit2), press(Key::F9)});
        check_semantic_editor(editor, true);
        for (std::size_t map = 0; map < 2; ++map)
            for (const auto mode : {DemoTilemapEditor::Mode::Literal, DemoTilemapEditor::Mode::Terrain})
                require(editor.history(map, mode) == HistoryState{}, "Fresh process inherited unsaved history");
        std::cout << "FRESH complete metadata +24 literal cells +24 semantic labels/meaning IDs/cardinal visuals; histories empty PASS\n";
    }

    namespace Variable = VariableDocuments;
    void check_variable(const DemoTilemapEditor& editor, const Storage::Snapshot& expected, std::size_t index, bool semantic) {
        require(
            (semantic ? editor.terrain_document(index).snapshot() : editor.document(index).snapshot()) == expected,
            "Variable complete snapshot/ordered metadata differs"
        );
        const std::array entries{
            KeyId{"terrain:one", {101}}, KeyId{"terrain:two", {203}}, KeyId{"terrain:three", {307}}, KeyId{"terrain:four", {409}}
        };
        const auto numeric = semantic ? LiteralOwner::from(expected, KeyTable(entries)) : nullptr;
        for (std::int64_t layer = 0; layer < 2; ++layer)
            for (std::uint32_t y = 0; y < expected.height; ++y)
                for (std::uint32_t x = 0; x < expected.width; ++x) {
                    const auto l = static_cast<std::size_t>(layer);
                    const auto& key = expected.layers[l].cells[std::size_t(y) * expected.width + x];
                    const auto& map = semantic ? numeric->map() : editor.map(index);
                    require(
                        map.extent() == Tiles::Extent{expected.width, expected.height} && map.layer_count() == 2,
                        "Variable numeric shape differs"
                    );
                    require(map.get({layer}, {x, y}).value == Variable::numeric(key, semantic), "Variable numeric meaning differs");
                    if (semantic) {
                        const auto& terrain = editor.terrain(index, {layer});
                        require(terrain.label({x, y}).value == Variable::meaning(key, true), "Variable opaque label differs");
                        require(
                            terrain.tiles().get({layer}, {x, y}).value == Variable::visual(expected, l, x, y),
                            "Variable full-grid cardinal oracle differs"
                        );
                    }
                }
    }
    void variable_documents(const std::filesystem::path& root, bool author) {
        std::size_t checked = 0;
        for (std::size_t shape = 0; shape < Variable::shapes.size(); ++shape) {
            const auto directory = root / ("domain-" + std::to_string(shape));
            if (author)
                require(std::filesystem::create_directory(directory), "Could not create owned process fixture directory");
            DemoTilemapEditor editor(directory);
            editor_events(editor, {});
            require(editor.set_view({0, -3072}, 0.125), "Variable full-footprint viewport failed");
            for (bool semantic : {false, true})
                for (std::size_t index = 0; index < 2; ++index) {
                    const auto size = Variable::shapes[(shape + index) % Variable::shapes.size()];
                    auto initial = Variable::source(size[0], size[1], semantic, index == 1);
                    auto saved = initial;
                    saved.layers[1].cells.back() = semantic ? "terrain:two" : "tilemap:red";
                    const auto path =
                        directory / (semantic ? (index ? "east-terrain.json" : "west-terrain.json") : (index ? "east.json" : "west.json"));
                    if (author)
                        Storage::save(path, initial);
                    else
                        require(Storage::load(path) == saved, "Fresh expected complete variable file differs");
                    editor_events(editor, {press(semantic ? Key::B : Key::V), press(index ? Key::Digit2 : Key::Digit1), press(Key::F9)});
                    check_variable(editor, author ? initial : saved, index, semantic);
                    if (author) {
                        const double ox = index ? 328 : 48, oy = index ? 208 : 240;
                        const double x = (ox + (double(size[0]) - 0.5) * 64) / 16;
                        const double y = (600 - (oy - (double(size[1]) - 0.5) * 48 + 3072) / 8) / 3;
                        editor_events(editor, {press(Key::T), press(semantic ? Key::Digit4 : Key::Digit6), paint(x, y), press(Key::F8)});
                        check_variable(editor, saved, index, semantic);
                        // A guaranteed DIFFERENT meaning, followed by undo/redo, then LOAD restores saved.
                        editor_events(editor, {press(Key::Digit3), paint(x, y), press(Key::F6)});
                        check_variable(editor, saved, index, semantic);
                        editor_events(editor, {press(Key::F7), press(Key::F9)});
                        check_variable(editor, saved, index, semantic);
                        require(
                            editor.history(index, semantic ? DemoTilemapEditor::Mode::Terrain : DemoTilemapEditor::Mode::Literal).undo == 0,
                            "Variable LOAD did not reset selected history"
                        );
                        require(Storage::load(path) == saved, "Variable save/load lost complete content");
                    }
                    checked += std::size_t(size[0]) * size[1] * 2;
                }
        }
        std::cout << (author ? "AUTHOR" : "FRESH") << ": five asymmetric/degenerate/max domains,20 mode/map documents; " << checked
                  << " complete cells + all keys/labels/numeric meanings/cardinal visuals/ordered metadata PASS\n";
    }

    void layered_documents(const std::filesystem::path& directory, bool author) {
        namespace Expected = GeneratedExpectations;
        auto final = Expected::base();
        final.layers[0].cells[64] = "terrain:four";
        final.layers[0].cells[65].reset();
        final.layers[1].cells[32] = final.layers[1].cells[33] = "terrain:one";
        DemoTilemapEditor editor(directory);
        editor_events(editor, {press(Key::B)});
        require(editor.set_view({0, -3072}, 0.125), "Layering viewport failed");
        const auto path = directory / "west-terrain.json";
        if (author) {
            editor.install_generated(Expected::recipe(), GeneratedReplace::DiscardCurrent);
            check_variable(editor, Expected::base(), 0, true);
            editor.regenerate_selected();
            require(editor.generated_state(0)->overlay.edits.empty(), "Default replay invented player edits");
            auto erase = paint(17, 67);
            erase.data = Events::Button{
                CE::Input::control_id(CE::Input::MouseButton::Right),
                Events::ButtonPhase::Press,
                CE::Input::Modifiers::None,
                -1,
                -1,
                Key::Unknown,
                CE::Input::MouseButton::Right,
                Events::Pointer{17, 67}
            };
            editor_events(editor, {press(Key::Digit6), paint(13, 67), paint(13, 67), erase});
            auto ground = final;
            ground.layers[1].cells = Expected::base().layers[1].cells;
            check_variable(editor, ground, 0, true);
            editor_events(editor, {press(Key::T), press(Key::Digit3), paint(9, 65), paint(13, 65)});
            check_variable(editor, final, 0, true);
            const auto held = *editor.generated_state(0);
            const std::vector<Patch::Edit> literal_edits{
                {"floor", 2, 2, "terrain:four"},
                {"floor", 3, 2, std::nullopt},
                {"overlay", 1, 1, "terrain:one"},
                {"overlay", 2, 1, "terrain:one"}
            };
            require(held.overlay.edits == literal_edits, "Final sparse changes differ from four literal addresses");
            require(held.overlay == Patch::diff(Expected::base(), final), "Final complete expected diff differs");
            editor_events(editor, {press(Key::F6)});
            check_variable(editor, ground, 0, true);
            const auto* before = &editor.terrain_document(0);
            editor.regenerate_selected();
            require(&editor.terrain_document(0) == before, "Validated replay invalidated borrowed owner");
            editor_events(editor, {press(Key::F7)});
            editor.regenerate_selected();
            check_variable(editor, final, 0, true);
            editor_events(editor, {press(Key::F8)});
            require(editor.generated_state(0)->overlay == held.overlay, "SAVE changed in-memory layering");
            require(Storage::load(path) == final, "Full saved layered semantics differ");
            std::cout << "RETAINED recipe=" << held.bytes.recipe << " base=" << held.bytes.base << " witness=" << held.bytes.witness
                      << " edits=" << held.bytes.edits << " history=" << held.bytes.history << " total=" << held.bytes.retained
                      << " full_snapshot=" << held.bytes.full_snapshot << " reserved_work=" << held.bytes.reserved_work
                      << " cave_storage=" << held.bytes.cave_storage << " cave_work=" << held.bytes.cave_work << '\n';
        } else {
            require(Storage::load(path) == final, "Fresh full layered Snapshot differs from independent literal expectations");
            editor_events(editor, {press(Key::F9)});
            check_variable(editor, final, 0, true);
            require(!editor.generated_state(0), "Fresh full Snapshot LOAD restored nonexistent recipe/provenance");
            require(editor.history(0, DemoTilemapEditor::Mode::Terrain).undo == 0, "Fresh load restored history");
            refuses<Error>(
                [&] { editor.regenerate_selected(); },
                [](const auto& error) { require(error.code == Refusal::Generation, "Wrong detached replay diagnostic"); }
            );
        }
        std::cout
            << (author ? "AUTHOR" : "FRESH")
            << ":31x19 real cave layering,1178 complete semantic keys/labels/meaning IDs/visuals/metadata PASS; full-snapshot fallback only\n";
    }

    void refusals(const std::filesystem::path& path) {
        const KeyTable table(keys);
        auto live = LiteralOwner::from(literal_base(), table);
        const auto* identity = live.get();
        auto held = literal_target();
        const auto check_error = [](const Refusal code) {
            return [=](const Error& error) { require(error.code == code, "Wrong private conversion refusal"); };
        };
        std::unique_ptr<LiteralOwner> owned;
        {
            auto transient = std::vector<KeyId>(keys.begin(), keys.end());
            const KeyTable local(transient);
            auto input = literal_base();
            owned = LiteralOwner::from(input, local);
            transient.clear();
            input.layers.clear();
            input.spawns.clear();
        }
        require(owned->snapshot() == literal_base(), "Owner retained borrowed table/snapshot data");
        for (const auto& invalid :
             {std::vector<KeyId>{{"proof:a", {0}}}, {{"proof:a", {1}}, {"proof:a", {2}}}, {{"proof:a", {1}}, {"proof:b", {1}}}, {}})
            refuses<Error>([&] { static_cast<void>(KeyTable(invalid)); }, check_error(Refusal::Table));
        const std::array overlong{KeyId{std::string(129, 'a'), {101}}};
        refuses<Error>([&] { static_cast<void>(KeyTable(overlong)); }, check_error(Refusal::Table));
        auto unknown = literal_base();
        unknown.layers[1].cells.back() = "proof:unknown";
        refuses<Error>([&] { live = LiteralOwner::from(unknown, table); }, check_error(Refusal::UnknownKey));
        require(live.get() == identity && live->snapshot() == literal_base(), "Refused adoption replaced prior owner");
        live->map().set({1}, {2, 1}, {99999}); // Map allows arbitrary IDs; reverse conversion must refuse.
        refuses<Error>([&] { held = live->snapshot(); }, check_error(Refusal::UnknownId));
        require(held == literal_target() && live->map().get({1}, {2, 1}).value == 99999, "Reverse refusal changed prior result/runtime");
        live->map().set({1}, {2, 1}, {1201});
        check_map(live->map(), {3, 2}, literal_base_ids);
        const auto original_map = live->map();
        live->map() = Tiles::Map({1, 1}, 1);
        refuses<Error>([&] { held = live->snapshot(); }, check_error(Refusal::Table));
        require(held == literal_target() && live->map().extent() == Tiles::Extent{1, 1}, "Shape refusal changed prior output/map");
        live->map() = original_map;
        const auto patch = Patch::diff(literal_base(), literal_target());
        const auto patch_copy = patch;
        auto result = literal_target();
        for (int change = 0; change < 7; ++change) {
            auto target = literal_target();
            if (change == 0)
                target.id = "proof:another";
            if (change == 1)
                std::swap(target.layers[0], target.layers[1]);
            if (change == 2) {
                target.layers[1].id = "overlay";
                target.spawns[1].layer = "overlay";
            }
            if (change == 3)
                std::swap(target.spawns[0], target.spawns[1]);
            if (change == 4)
                target.spawns[0].x = 1;
            if (change == 5)
                target.spawns[0].name = "arrival";
            if (change == 6) {
                target.width = 2;
                target.height = 3;
                target.spawns[1].x = 1;
            }
            Storage::validate(target); // Establish valid setup before metadata refusal.
            const auto original = target;
            refuses<Patch::Error>(
                [&] { result = Patch::apply(literal_base(), Patch::diff(literal_base(), target)); },
                [](const auto& error) {
                    require(error.code == Patch::Code::UnsupportedChange && error.operation == "diff", "Wrong metadata refusal");
                }
            );
            require(target == original && result == literal_target(), "Metadata refusal mutated inputs/held result");
        }
        for (int change = 0; change < 3; ++change) {
            auto mismatch = literal_base();
            if (change == 0)
                mismatch.layers[0].cells[5] = "proof:earth";
            if (change == 1)
                std::swap(mismatch.layers[0], mismatch.layers[1]);
            if (change == 2)
                mismatch.spawns[0].x = 1;
            Storage::validate(mismatch);
            refuses<Patch::Error>(
                [&] { result = Patch::apply(mismatch, patch); },
                [](const auto& error) { require(error.code == Patch::Code::BaseMismatch, "Wrong exact-base refusal"); }
            );
        }
        auto duplicate = patch;
        duplicate.edits.push_back(duplicate.edits[0]);
        refuses<Patch::Error>(
            [&] { result = Patch::apply(literal_base(), duplicate); },
            [](const auto& error) { require(error.code == Patch::Code::InvalidEdit, "Storage duplicates did not reject"); }
        );
        require(patch == patch_copy && result == literal_target(), "Rejected patch changed earlier patch/result");

        auto invalid_shape = literal_base();
        invalid_shape.width = 0;
        refuses<Storage::Error>(
            [&] { live = LiteralOwner::from(invalid_shape, table); },
            [](const auto& error) { require(error.code == Storage::Code::InvalidValue, "Wrong invalid shape error"); }
        );
        Storage::Snapshot over_patch{
            "proof:large",
            256,
            129,
            {{"a", std::vector<std::optional<std::string>>(256 * 129)}, {"b", std::vector<std::optional<std::string>>(256 * 129)}},
            {}
        };
        Storage::validate(over_patch);
        refuses<Error>([&] { live = LiteralOwner::from(over_patch, table); }, check_error(Refusal::Limit));
        Storage::Snapshot many{"proof:changes", 33, 32, {{"ground", std::vector<std::optional<std::string>>(33 * 32)}}, {}};
        auto too_many = many;
        std::fill_n(too_many.layers[0].cells.begin(), 1025, "proof:earth");
        refuses<Patch::Error>(
            [&] { static_cast<void>(Patch::diff(many, too_many)); },
            [](const auto& error) { require(error.code == Patch::Code::LimitExceeded, "Truncated >1024 final changes"); }
        );
        require(live.get() == identity && live->snapshot() == literal_base(), "Bounds refusal changed adopted owner");

        auto terrain = TerrainOwner::from(cave_base(), table, {0}, semantics, rule(), targets());
        const auto* terrain_identity = terrain.get();
        auto semantic_overlong = semantics;
        semantic_overlong[1].key.assign(129, 'b');
        refuses<Error>(
            [&] { terrain = TerrainOwner::from(cave_base(), table, {0}, semantic_overlong, rule(), targets()); },
            check_error(Refusal::Table)
        );
        const auto terrain_before = terrain->snapshot();
        const auto visual_before = terrain->terrain().tiles();
        const std::array unknown_label{Tiles::TerrainEdit{{2, 1}, {999}}, Tiles::TerrainEdit{{2, 1}, {11}}};
        refuses<Error>([&] { static_cast<void>(terrain->edit_batch(unknown_label, {2, 5, 100})); }, check_error(Refusal::UnknownLabel));
        const std::array bad_point{Tiles::TerrainEdit{{2, 1}, {11}}, Tiles::TerrainEdit{{5, 3}, {23}}};
        refuses<bad_request>([&] { static_cast<void>(terrain->edit_batch(bad_point, {2, 10, 200})); }, [](const auto&) {});
        auto incomplete = rule();
        incomplete.cases.erase(4); // Initial geometry is valid; edit makes east floor require South-only mask4.
        auto late = TerrainOwner::from(cave_base(), table, {0}, semantics, incomplete, targets());
        require(late->snapshot() == cave_base(), "Late failure setup is invalid");
        const std::array one{Tiles::TerrainEdit{{2, 1}, {11}}};
        refuses<bad_request>(
            [&] { static_cast<void>(late->edit_batch(one, {1, 5, 100})); },
            [](const auto& error) {
                require(std::string_view(error.what()).find("(3,1)") != std::string_view::npos, "Not the later east neighbor refusal");
            }
        );
        auto unmapped = targets();
        unmapped[6] = {0}; // Required by the initial corner.
        refuses<bad_request>(
            [&] { terrain = TerrainOwner::from(cave_base(), table, {0}, semantics, rule(), unmapped); }, [](const auto&) {}
        );
        require(
            terrain.get() == terrain_identity && terrain->snapshot() == terrain_before && late->snapshot() == cave_base(),
            "Terrain refusal published partial semantics or adopted replacement"
        );
        for (std::int64_t y = 0; y < 4; ++y)
            for (std::int64_t x = 0; x < 5; ++x)
                for (std::int64_t layer = 0; layer < 2; ++layer) {
                    require(
                        terrain->terrain().tiles().get({layer}, {x, y}) == visual_before.get({layer}, {x, y}),
                        "Prior terrain visuals changed"
                    );
                    require(
                        late->terrain().tiles().get({layer}, {x, y}) == visual_before.get({layer}, {x, y}),
                        "Late failure partially derived visuals"
                    );
                }
        Storage::save(path, literal_base());
        const auto predecessor = bytes(path);
        auto invalid_save = literal_target();
        invalid_save.layers[1].cells[5] = "not-qualified";
        refuses<Storage::Error>(
            [&] { Storage::save(path, invalid_save); },
            [](const auto& error) {
                require(error.code == Storage::Code::InvalidValue && error.operation == "encode", "Wrong prepublication error");
            }
        );
        require(bytes(path) == predecessor && Storage::load(path) == literal_base(), "Refused save replaced predecessor");
        require(result == literal_target() && live->snapshot() == literal_base(), "Failures corrupted previous results");
        std::cout
            << "Refusals preserve adopted owners/full snapshots/visuals/results and predecessor bytes; runtime/file boundaries separate PASS\n";
    }
}

int main(const int argc, char** argv) {
    try {
        require(argc == 3, "Usage: tests-world-consumer MODE OWNED_PATH");
        const std::string_view mode = argv[1];
        const std::filesystem::path path = argv[2];
        if (mode == "literal-author")
            literal_author(path);
        else if (mode == "literal-verify")
            literal_verify(path, false);
        else if (mode == "literal-base-verify")
            literal_verify(path, true);
        else if (mode == "generated-author")
            generated_author(path);
        else if (mode == "generated-verify")
            generated_verify(path);
        else if (mode == "refusals")
            refusals(path);
        else if (mode == "editor-author")
            editor_author(path);
        else if (mode == "editor-verify")
            editor_verify(path);
        else if (mode == "semantic-author")
            semantic_author(path);
        else if (mode == "semantic-verify")
            semantic_verify(path);
        else if (mode == "history-author")
            history_author(path);
        else if (mode == "history-verify")
            history_verify(path);
        else if (mode == "variable-author")
            variable_documents(path, true);
        else if (mode == "variable-verify")
            variable_documents(path, false);
        else if (mode == "layered-author")
            layered_documents(path, true);
        else if (mode == "layered-verify")
            layered_documents(path, false);
        else
            throw std::runtime_error("Unknown world mode");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "World CPU proof failed: " << error.what() << '\n';
        return 1;
    }
}
