#include "world-document.h"

#include <cheryl/core/resources/map-storage/cell-patch.h>

#include <algorithm>
#include <limits>
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
    TerrainDocument::TerrainDocument(
        KeyTable keys,
        std::vector<SemanticKey> semantics,
        CE::Assets::Tilesets::Rule rule,
        std::vector<Tiles::TileId> targets,
        std::array<std::unique_ptr<TerrainOwner>, 2> layers
    )
    : keys_(std::move(keys)),
      semantics_(std::move(semantics)),
      rule_(std::move(rule)),
      targets_(std::move(targets)),
      layers_(std::move(layers)) {}

    std::unique_ptr<TerrainDocument> TerrainDocument::from(
        const Storage::Snapshot& snapshot,
        const KeyTable& keys,
        const std::span<const SemanticKey> semantics,
        const CE::Assets::Tilesets::Rule& rule,
        const std::span<const Tiles::TileId> targets
    ) {
        admit(snapshot);
        // Private editor kind convention, also meaningful for an all-null file.
        // This is NOT a new storage property: v1 still treats map ID as opaque.
        if (!snapshot.id.starts_with("terrain:") || snapshot.width != 3 || snapshot.height != 2 || snapshot.layers.size() != 2)
            throw Error(Refusal::Table, "Terrain document requires terrain: map ID, 3x2 cells and two ordered layers");
        std::array<std::unique_ptr<TerrainOwner>, 2> layers;
        for (std::int64_t layer = 0; layer < 2; ++layer)
            layers[static_cast<std::size_t>(layer)] = TerrainOwner::from(snapshot, keys, {layer}, semantics, rule, targets);
        return std::unique_ptr<TerrainDocument>(
            new TerrainDocument(keys, {semantics.begin(), semantics.end()}, rule, {targets.begin(), targets.end()}, std::move(layers))
        );
    }
    std::unique_ptr<TerrainDocument> TerrainDocument::replacement(const Storage::Snapshot& snapshot) const {
        return from(snapshot, keys_, semantics_, rule_, targets_);
    }
    TerrainOwner& TerrainDocument::owner(const Tiles::LayerIndex layer) {
        if (layer.value < 0 || layer.value >= 2)
            throw Error(Refusal::Table, "Invalid terrain document layer");
        return *layers_[static_cast<std::size_t>(layer.value)];
    }
    const TerrainOwner& TerrainDocument::owner(const Tiles::LayerIndex layer) const {
        if (layer.value < 0 || layer.value >= 2)
            throw Error(Refusal::Table, "Invalid terrain document layer");
        return *layers_[static_cast<std::size_t>(layer.value)];
    }
    const Tiles::TerrainMap& TerrainDocument::terrain(const Tiles::LayerIndex layer) const {
        return owner(layer).terrain();
    }
    std::vector<Tiles::Point> TerrainDocument::edit_batch(
        const Tiles::LayerIndex layer,
        const std::span<const Tiles::TerrainEdit> edits,
        const Tiles::TerrainBatchLimits limits
    ) {
        return owner(layer).edit_batch(edits, limits);
    }
    Storage::Snapshot TerrainDocument::snapshot() const {
        auto result = layers_[0]->snapshot();
        // Complementary numeric layers inside each owner are construction-time
        // meaning, not live visuals. Export only each owner's semantic layer.
        auto second = layers_[1]->snapshot();
        result.layers[1] = std::move(second.layers[1]);
        admit(result);
        return result;
    }
    DocumentHistory::DocumentHistory(const HistoryLimits limits)
    : limits_(limits) {
        if (limits.entries > 256 || limits.bytes > Storage::max_document_bytes)
            throw Error(Refusal::History, "History limits exceed 256 commands or 8 MiB canonical bytes per document");
    }
    HistoryState DocumentHistory::state() const {
        return {cursor_, entries_.size() - cursor_, bytes_};
    }
    DocumentHistory DocumentHistory::recorded(Storage::Snapshot before, Storage::Snapshot after) const {
        const auto before_bytes = Storage::encode(before).size();
        const auto after_bytes = Storage::encode(after).size();
        if (!entries_.empty()) {
            const auto& expected = cursor_ == 0 ? entries_.front().before : entries_[cursor_ - 1].after;
            if (before != expected)
                throw Error(Refusal::History, "Document content differs from the history witness");
        }
        if (before == after)
            return *this; // Valid no-op preserves redo and consumes no retained budget.
        if (after_bytes > std::numeric_limits<std::size_t>::max() - before_bytes)
            throw Error(Refusal::History, "History command byte charge overflows size_t");
        const auto cost = before_bytes + after_bytes;
        if (!limits_.entries || cost > limits_.bytes)
            throw Error(Refusal::History, "History cannot retain this command within its entry/byte cap");
        std::size_t retained = 0;
        for (std::size_t i = 0; i < cursor_; ++i) {
            if (entries_[i].bytes > std::numeric_limits<std::size_t>::max() - retained)
                throw Error(Refusal::History, "History retained byte charge overflows size_t");
            retained += entries_[i].bytes;
        }
        std::size_t oldest = 0;
        while (cursor_ - oldest >= limits_.entries || retained > limits_.bytes - cost)
            retained -= entries_[oldest++].bytes;
        DocumentHistory candidate(limits_);
        candidate.entries_.reserve(cursor_ - oldest + 1); // <=256, never a caller-sized allocation probe.
        for (auto i = oldest; i < cursor_; ++i)
            candidate.entries_.push_back(entries_[i]);
        candidate.entries_.push_back({std::move(before), std::move(after), cost});
        candidate.cursor_ = candidate.entries_.size();
        candidate.bytes_ = retained + cost; // Checked above against limits_.bytes.
        return candidate;                   // Redo/eviction happens only in this owned candidate.
    }
    const Storage::Snapshot& DocumentHistory::target(const bool undo) const {
        if (undo ? cursor_ == 0 : cursor_ == entries_.size())
            throw Error(Refusal::History, undo ? "Document undo history is empty" : "Document redo history is empty");
        return undo ? entries_[cursor_ - 1].before : entries_[cursor_].after;
    }
    DocumentHistory DocumentHistory::stepped(const bool undo, const Storage::Snapshot& current) const {
        static_cast<void>(target(undo));
        const auto& expected = undo ? entries_[cursor_ - 1].after : entries_[cursor_].before;
        if (current != expected)
            throw Error(Refusal::History, "Document content differs from the history witness");
        auto candidate = *this;
        if (undo)
            --candidate.cursor_;
        else
            ++candidate.cursor_;
        return candidate;
    }
    void DocumentHistory::swap(DocumentHistory& other) noexcept {
        using std::swap;
        swap(limits_, other.limits_);
        entries_.swap(other.entries_);
        swap(cursor_, other.cursor_);
        swap(bytes_, other.bytes_);
    }
}
