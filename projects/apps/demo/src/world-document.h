#pragma once

#include <cheryl/core/resources/map-storage/file-storage.h>
#include <cheryl/maps/terrain-map.h>

#include <memory>
#include <span>

// Private integration policy. These meanings, keys, limits and owners are not
// Engine/SDK APIs. Serialize access; borrowed map/terrain views expire with owner.
namespace WorldProof {
    namespace Storage = CE::Maps;
    namespace Tiles = CE::Tilemaps;
    enum class Refusal { Table, UnknownKey, UnknownId, UnknownLabel, Limit };
    struct Error final : std::runtime_error {
        Refusal code;
        Error(Refusal code, const std::string& detail);
    };
    struct KeyId {
        std::string key;
        Tiles::TileId id;
    };
    class KeyTable {
        std::vector<KeyId> entries_;

    public:
        explicit KeyTable(std::span<const KeyId> entries);
        [[nodiscard]] Tiles::TileId id(const std::optional<std::string>& key) const;
        [[nodiscard]] std::optional<std::string> key(Tiles::TileId id) const;
    };
    struct Metadata {
        std::string id;
        Tiles::Extent extent;
        std::vector<std::string> layers;
        std::vector<Storage::Spawn> spawns;
    };
    class LiteralOwner {
        Metadata metadata_;
        KeyTable keys_;
        Tiles::Map map_;

        LiteralOwner(Metadata metadata, KeyTable keys, Tiles::Map map);

    public:
        [[nodiscard]] static std::unique_ptr<LiteralOwner> from(const Storage::Snapshot& snapshot, const KeyTable& keys);
        [[nodiscard]] static std::unique_ptr<LiteralOwner> load(const std::filesystem::path& path, const KeyTable& keys);
        [[nodiscard]] Tiles::Map& map() { return map_; }
        [[nodiscard]] const Tiles::Map& map() const { return map_; }
        [[nodiscard]] Storage::Snapshot snapshot() const;
    };
    struct SemanticKey {
        std::string key;
        Tiles::TerrainLabel label;
    };
    class TerrainOwner {
        Metadata metadata_;
        KeyTable keys_;
        std::vector<SemanticKey> semantics_;
        Tiles::LayerIndex layer_;
        std::unique_ptr<Tiles::TerrainMap> terrain_;

        TerrainOwner(
            Metadata metadata,
            KeyTable keys,
            std::vector<SemanticKey> semantics,
            Tiles::LayerIndex layer,
            std::unique_ptr<Tiles::TerrainMap> terrain
        );
        void validate_label(Tiles::TerrainLabel label) const;

    public:
        [[nodiscard]] static std::unique_ptr<TerrainOwner> from(
            const Storage::Snapshot& snapshot,
            const KeyTable& keys,
            Tiles::LayerIndex layer,
            std::span<const SemanticKey> semantics,
            const CE::Assets::Tilesets::Rule& rule,
            std::span<const Tiles::TileId> targets
        );
        [[nodiscard]] const Tiles::TerrainMap& terrain() const { return *terrain_; }
        [[nodiscard]] std::vector<Tiles::Point> edit_batch(std::span<const Tiles::TerrainEdit> edits, Tiles::TerrainBatchLimits limits);
        [[nodiscard]] Storage::Snapshot snapshot() const;
    };
}
