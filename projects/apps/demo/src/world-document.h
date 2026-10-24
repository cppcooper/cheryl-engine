#pragma once

#include <cheryl/core/resources/map-storage/file-storage.h>
#include <cheryl/maps/terrain-map.h>

#include <array>
#include <memory>
#include <span>

// Private integration policy. These meanings, keys, limits and owners are not
// Engine/SDK APIs. Serialize access; borrowed map/terrain views expire with owner.
namespace WorldProof {
    namespace Storage = CE::Maps;
    namespace Tiles = CE::Tilemaps;
    enum class Refusal { Table, UnknownKey, UnknownId, UnknownLabel, Limit, History, Generation };
    struct Error final : std::runtime_error {
        Refusal code;
        Error(Refusal code, const std::string& detail);
    };
    inline constexpr std::uint32_t editor_max_axis = 64;
    inline constexpr std::size_t editor_layer_cells = 8192;
    // Full storage/adapter validation, then private finite-editor admission.
    // General LiteralOwner/TerrainOwner conversions retain their older bounds.
    void admit_editor_document(const Storage::Snapshot& snapshot);
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
    [[nodiscard]] KeyTable editor_meaning_ids();
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
    [[nodiscard]] std::span<const SemanticKey> editor_semantics();
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
    // Private two-layer semantic document. Each TerrainOwner owns its copied
    // rule/mapping; rebuilding BOTH precedes outer pointer adoption. Borrowed
    // layer views expire on document replacement. No visual-ID reverse lookup.
    class TerrainDocument {
        KeyTable keys_;
        std::vector<SemanticKey> semantics_;
        CE::Assets::Tilesets::Rule rule_;
        std::vector<Tiles::TileId> targets_;
        std::array<std::unique_ptr<TerrainOwner>, 2> layers_;

        TerrainDocument(
            KeyTable keys,
            std::vector<SemanticKey> semantics,
            CE::Assets::Tilesets::Rule rule,
            std::vector<Tiles::TileId> targets,
            std::array<std::unique_ptr<TerrainOwner>, 2> layers
        );
        [[nodiscard]] TerrainOwner& owner(Tiles::LayerIndex layer);
        [[nodiscard]] const TerrainOwner& owner(Tiles::LayerIndex layer) const;

    public:
        [[nodiscard]] static std::unique_ptr<TerrainDocument> from(
            const Storage::Snapshot& snapshot,
            const KeyTable& keys,
            std::span<const SemanticKey> semantics,
            const CE::Assets::Tilesets::Rule& rule,
            std::span<const Tiles::TileId> targets
        );
        [[nodiscard]] std::unique_ptr<TerrainDocument> replacement(const Storage::Snapshot& snapshot) const;
        [[nodiscard]] const Tiles::TerrainMap& terrain(Tiles::LayerIndex layer) const;
        [[nodiscard]] std::vector<Tiles::Point>
        edit_batch(Tiles::LayerIndex layer, std::span<const Tiles::TerrainEdit> edits, Tiles::TerrainBatchLimits limits);
        [[nodiscard]] Storage::Snapshot snapshot() const;
    };
    struct HistoryLimits {
        std::size_t entries = 32;
        std::size_t bytes = 1024 * 1024;
    };
    struct HistoryState {
        std::size_t undo = 0;
        std::size_t redo = 0;
        std::size_t bytes = 0;
        [[nodiscard]] bool operator==(const HistoryState&) const = default;
    };
    // One private document's bounded timeline; a cursor represents both stacks.
    // Charge is BOTH complete canonical-v1 encodings, not allocator/RSS usage.
    // Reconstruction configuration belongs to the immutable current owner epoch;
    // successful LOAD replaces that epoch and clears only its own history.
    class DocumentHistory {
        struct Entry {
            Storage::Snapshot before;
            Storage::Snapshot after;
            std::size_t bytes;
        };
        HistoryLimits limits_;
        std::vector<Entry> entries_;
        std::size_t cursor_ = 0;
        std::size_t bytes_ = 0;

    public:
        explicit DocumentHistory(HistoryLimits limits = {});
        DocumentHistory(const DocumentHistory&) = default;
        DocumentHistory& operator=(const DocumentHistory&) = default;
        [[nodiscard]] HistoryState state() const;
        [[nodiscard]] DocumentHistory recorded(Storage::Snapshot before, Storage::Snapshot after) const;
        [[nodiscard]] const Storage::Snapshot& target(bool undo) const;
        [[nodiscard]] DocumentHistory stepped(bool undo, const Storage::Snapshot& current) const;
        void swap(DocumentHistory& other) noexcept;
    };
}
