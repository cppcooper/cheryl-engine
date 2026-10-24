#include "bridge.h"

#include <cheryl/core/resources/map-storage/cell-patch.h>

#include <algorithm>
#include <utility>

namespace WorldProof {
    namespace {
        void admit(const Storage::Snapshot& snapshot) {
            Storage::validate(snapshot);
            // Storage and patch limits are stricter than CPU Map. Divide before
            // multiplying; never clamp/truncate a caller's admitted domain.
            if (snapshot.width > Storage::CellPatches::max_base_cells / snapshot.height / snapshot.layers.size())
                throw Error(Refusal::Limit, "Private world adapter exceeds patch cell budget");
        }
        Metadata metadata(const Storage::Snapshot& snapshot) {
            Metadata result{snapshot.id, {snapshot.width, snapshot.height}, {}, snapshot.spawns};
            for (const auto& layer : snapshot.layers)
                result.layers.push_back(layer.id);
            return result;
        }
        Storage::Snapshot shape(const Metadata& meta, const Tiles::Map& map) {
            if (map.extent() != meta.extent || map.layer_count() != meta.layers.size())
                throw Error(Refusal::Table, "Numeric map assignment changed retained metadata shape");
            Storage::Snapshot result{meta.id, map.extent().columns, map.extent().rows, {}, meta.spawns};
            const auto count = std::size_t(result.width) * result.height;
            for (const auto& layer : meta.layers)
                result.layers.push_back({layer, std::vector<std::optional<std::string>>(count)});
            return result;
        }
        template <class F> void cells(const Tiles::Map& map, F&& operation) {
            for (std::uint32_t layer = 0; layer < map.layer_count(); ++layer)
                for (std::uint32_t row = 0; row < map.extent().rows; ++row)
                    for (std::uint32_t column = 0; column < map.extent().columns; ++column)
                        operation(Tiles::LayerIndex{layer}, Tiles::Point{column, row}, std::size_t(row) * map.extent().columns + column);
        }
    }
    Error::Error(const Refusal code, const std::string& detail)
    : std::runtime_error(detail), code(code) {}

    KeyTable::KeyTable(const std::span<const KeyId> entries) {
        if (entries.empty() || entries.size() > 256)
            throw Error(Refusal::Table, "Private key table requires 1..256 entries");
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].key.size() > Storage::max_key_bytes)
                throw Error(Refusal::Table, "Persistent key exceeds storage byte bound");
            if (!entries[i].id.value)
                throw Error(Refusal::Table, "Zero is reserved for null, not a named key");
            for (std::size_t j = 0; j < i; ++j)
                if (entries[i].key == entries[j].key || entries[i].id == entries[j].id)
                    throw Error(Refusal::Table, "Ambiguous key or numeric ID");
            Storage::validate({"proof:table", 1, 1, {{"keys", {entries[i].key}}}, {}});
        }
        entries_.assign(entries.begin(), entries.end());
    }
    Tiles::TileId KeyTable::id(const std::optional<std::string>& key) const {
        if (!key)
            return {0};
        for (const auto& entry : entries_)
            if (entry.key == *key)
                return entry.id;
        throw Error(Refusal::UnknownKey, "Unmapped persistent key: " + *key);
    }
    std::optional<std::string> KeyTable::key(const Tiles::TileId id) const {
        if (!id.value)
            return std::nullopt;
        for (const auto& entry : entries_)
            if (entry.id == id)
                return entry.key;
        throw Error(Refusal::UnknownId, "Unmapped numeric TileId: " + std::to_string(id.value));
    }
    LiteralOwner::LiteralOwner(Metadata meta, KeyTable keys, Tiles::Map map)
    : metadata_(std::move(meta)), keys_(std::move(keys)), map_(std::move(map)) {}

    std::unique_ptr<LiteralOwner> LiteralOwner::from(const Storage::Snapshot& snapshot, const KeyTable& keys) {
        admit(snapshot);
        // Validate EVERY key before allocating/populating a replacement map.
        for (const auto& layer : snapshot.layers)
            for (const auto& key : layer.cells)
                static_cast<void>(keys.id(key));
        Tiles::Map candidate({snapshot.width, snapshot.height}, static_cast<std::uint32_t>(snapshot.layers.size()));
        cells(candidate, [&](const auto layer, const auto point, const auto index) {
            candidate.set(layer, point, keys.id(snapshot.layers[static_cast<std::size_t>(layer.value)].cells[index]));
        });
        return std::unique_ptr<LiteralOwner>(new LiteralOwner(metadata(snapshot), keys, std::move(candidate)));
    }
    std::unique_ptr<LiteralOwner> LiteralOwner::load(const std::filesystem::path& path, const KeyTable& keys) {
        return from(Storage::load(path), keys);
    }
    Storage::Snapshot LiteralOwner::snapshot() const {
        // A direct Map edit can introduce an unmapped ID. Preflight before any
        // candidate is adopted by the caller; the runtime edit itself is separate.
        if (map_.extent() != metadata_.extent || map_.layer_count() != metadata_.layers.size())
            throw Error(Refusal::Table, "Numeric map assignment changed retained metadata shape");
        cells(map_, [&](const auto layer, const auto point, const auto) { static_cast<void>(keys_.key(map_.get(layer, point))); });
        auto result = shape(metadata_, map_);
        cells(map_, [&](const auto layer, const auto point, const auto index) {
            result.layers[static_cast<std::size_t>(layer.value)].cells[index] = keys_.key(map_.get(layer, point));
        });
        admit(result);
        return result;
    }
    TerrainOwner::TerrainOwner(
        Metadata meta,
        KeyTable keys,
        std::vector<SemanticKey> semantics,
        const Tiles::LayerIndex layer,
        std::unique_ptr<Tiles::TerrainMap> terrain
    )
    : metadata_(std::move(meta)), keys_(std::move(keys)), semantics_(std::move(semantics)), layer_(layer), terrain_(std::move(terrain)) {}

    std::unique_ptr<TerrainOwner> TerrainOwner::from(
        const Storage::Snapshot& snapshot,
        const KeyTable& keys,
        const Tiles::LayerIndex layer,
        const std::span<const SemanticKey> semantics,
        const CE::Assets::Tilesets::Rule& rule,
        const std::span<const Tiles::TileId> targets
    ) {
        auto literal = LiteralOwner::from(snapshot, keys);
        if (layer.value < 0 || static_cast<std::size_t>(layer.value) >= snapshot.layers.size())
            throw Error(Refusal::Table, "Invalid semantic layer");
        if (semantics.empty() || semantics.size() > 256)
            throw Error(Refusal::Table, "Semantic table requires 1..256 entries");
        for (std::size_t i = 0; i < semantics.size(); ++i) {
            if (semantics[i].key.size() > Storage::max_key_bytes)
                throw Error(Refusal::Table, "Semantic key exceeds storage byte bound");
            static_cast<void>(keys.id(semantics[i].key));
            if (!semantics[i].label.value)
                throw Error(Refusal::Table, "Zero label is reserved for null");
            for (std::size_t j = 0; j < i; ++j)
                if (semantics[i].key == semantics[j].key || semantics[i].label == semantics[j].label)
                    throw Error(Refusal::Table, "Ambiguous semantic key or label");
        }
        const auto& source = snapshot.layers[static_cast<std::size_t>(layer.value)].cells;
        std::vector<Tiles::TerrainLabel> labels;
        labels.reserve(source.size());
        for (const auto& key : source) {
            if (!key) {
                labels.push_back({0});
                continue;
            }
            const auto found = std::ranges::find(semantics, *key, &SemanticKey::key);
            if (found == semantics.end())
                throw Error(Refusal::UnknownKey, "Unknown terrain meaning: " + *key);
            labels.push_back(found->label);
        }
        auto candidate = std::make_unique<Tiles::TerrainMap>(literal->map(), layer, labels, rule, targets);
        return std::unique_ptr<TerrainOwner>(
            new TerrainOwner(metadata(snapshot), keys, {semantics.begin(), semantics.end()}, layer, std::move(candidate))
        );
    }
    void TerrainOwner::validate_label(const Tiles::TerrainLabel label) const {
        if (label.value && std::ranges::find(semantics_, label, &SemanticKey::label) == semantics_.end())
            throw Error(Refusal::UnknownLabel, "Unknown semantic label: " + std::to_string(label.value));
    }
    std::vector<Tiles::Point>
    TerrainOwner::edit_batch(const std::span<const Tiles::TerrainEdit> edits, const Tiles::TerrainBatchLimits limits) {
        if (edits.size() > limits.requests)
            throw Error(Refusal::Limit, "Terrain requests exceed caller cap");
        for (const auto& edit : edits)
            validate_label(edit.label); // Even overwritten requests must be representable.
        return terrain_->edit_batch(edits, limits);
    }
    Storage::Snapshot TerrainOwner::snapshot() const {
        auto result = shape(metadata_, terrain_->tiles());
        cells(terrain_->tiles(), [&](const auto layer, const auto point, const auto index) {
            auto& key = result.layers[static_cast<std::size_t>(layer.value)].cells[index];
            if (layer != layer_)
                key = keys_.key(terrain_->tiles().get(layer, point));
            else {
                const auto label = terrain_->label(point);
                validate_label(label);
                if (label.value)
                    key = std::ranges::find(semantics_, label, &SemanticKey::label)->key;
            }
        });
        admit(result);
        return result; // Semantic keys, NEVER selected/animated visual IDs.
    }
}
