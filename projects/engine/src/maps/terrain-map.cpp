#include <maps/terrain-map.h>

#include <internals/exceptions.h>

#include <algorithm>
#include <array>
#include <format>
#include <limits>
#include <map>
#include <set>

namespace CE::Tilemaps {
    namespace {
        namespace Maps = CE::Tilemaps;
        namespace Tiles = CE::Assets::Tilesets;
        namespace Select = Tiles::Selection;
        constexpr std::array<Maps::Point, 4> offsets{Maps::Point{0, -1}, {1, 0}, {0, 1}, {-1, 0}};

        const Tiles::BitmaskRule& checked_rule(const Tiles::Rule& rule) {
            const auto* cardinal = std::get_if<Tiles::BitmaskRule>(&rule);
            if (!cardinal || cardinal->type != Tiles::BitmaskType::FourNeighbor)
                throw CE::Exceptions::invalid_args(CE_HERE, "Terrain consumer supports only four-neighbor bitmask rules");
            // Reuse the authoritative direction/type validation even when every
            // semantic cell is empty. An absent case is allowed until needed.
            static_cast<void>(Select::select(*cardinal, [](Select::Site) { return Select::Sample{Select::SampleState::Known, 0}; }, {}));
            const auto maximum = (std::uint32_t{1} << cardinal->bit_order.size()) - 1;
            for (const auto& [mask, target] : cardinal->cases) {
                static_cast<void>(target);
                if (mask > maximum)
                    throw CE::Exceptions::invalid_args(CE_HERE, "Terrain mask uses bits absent from bit_order");
            }
            return *cardinal;
        }

        const Maps::Map& checked_base(
            const Maps::Map& base,
            const Maps::LayerIndex layer,
            const std::span<const TerrainLabel> labels,
            const Tiles::Rule& rule,
            const std::span<const Maps::TileId> mapping
        ) {
            const auto extent = base.extent();
            if (!extent.columns || !extent.rows || extent.rows > std::numeric_limits<std::size_t>::max() / extent.columns)
                throw CE::Exceptions::invalid_args(CE_HERE, "Terrain needs a finite nonempty addressable map");
            const auto count = static_cast<std::size_t>(extent.columns) * extent.rows;
            if (count != labels.size() || count > std::vector<TerrainLabel>{}.max_size() ||
                mapping.size() > std::vector<Maps::TileId>{}.max_size())
                throw CE::Exceptions::invalid_args(CE_HERE, "Terrain label count must match the map and fit owned storage");
            static_cast<void>(base.get(layer, {0, 0}));
            static_cast<void>(checked_rule(rule));
            return base; // Checks precede copying/allocating consumer storage.
        }
        struct Work {
            std::size_t remaining;
            void take() {
                if (!remaining)
                    throw CE::Exceptions::bad_request(CE_HERE, "Terrain batch work cap exhausted");
                --remaining;
            }
        };

        template <class At, class Inside>
        Maps::TileId derive(
            const Maps::Point point,
            const Tiles::BitmaskRule& rule,
            const std::vector<Maps::TileId>& target_to_id,
            const At& at,
            const Inside& inside,
            Work* work = nullptr
        ) {
            if (work)
                work->take();
            const auto center = at(point);
            if (center.value == 0)
                return {0};
            const Select::Sampler sampler = [&](const Select::Site site) {
                if (work)
                    work->take();
                Maps::Point offset;
                switch (site.direction) {
                    case Tiles::Direction::North:
                        offset = offsets[0];
                        break;
                    case Tiles::Direction::East:
                        offset = offsets[1];
                        break;
                    case Tiles::Direction::South:
                        offset = offsets[2];
                        break;
                    case Tiles::Direction::West:
                        offset = offsets[3];
                        break;
                    default:
                        throw CE::Exceptions::invalid_args(CE_HERE, "Terrain sampler supports cardinal cells only");
                }
                // point is already in a uint32-bounded domain; +/-1 fits int64.
                const Maps::Point neighbor{point.column + offset.column, point.row + offset.row};
                return inside(neighbor) ? Select::Sample{Select::SampleState::Known, at(neighbor).value}
                                        : Select::Sample{Select::SampleState::Outside, 0};
            };
            const auto result = Select::select(rule, sampler, {.terrain = center.value, .outside = Select::Fallback::Empty});
            if (std::holds_alternative<Select::Failure>(result))
                throw CE::Exceptions::bad_request(
                    CE_HERE, std::format("Terrain selection missing rule at ({},{}) for label {}", point.column, point.row, center.value)
                );
            const auto target = std::get<CE::Assets::Grids::CellIndex>(result);
            if (target >= target_to_id.size() || target_to_id[target].value == 0)
                throw CE::Exceptions::bad_request(
                    CE_HERE, std::format("Terrain target {} is outside the mapping or unmapped at ({},{})", target, point.column, point.row)
                );
            return target_to_id[target];
        }
    }

    TerrainMap::TerrainMap(
        const Maps::Map& base,
        const Maps::LayerIndex layer,
        const std::span<const TerrainLabel> labels,
        const Tiles::Rule& rule,
        const std::span<const Maps::TileId> target_to_id
    )
    : tiles_(checked_base(base, layer, labels, rule, target_to_id)),
      layer_(layer),
      labels_(labels.begin(), labels.end()),
      rule_(std::get<Tiles::BitmaskRule>(rule)),
      target_to_id_(target_to_id.begin(), target_to_id.end()) {
        std::vector<Maps::TileId> derived;
        derived.reserve(labels_.size());
        const auto extent = tiles_.extent();
        for (std::int64_t row = 0; row < extent.rows; ++row)
            for (std::int64_t column = 0; column < extent.columns; ++column)
                derived.push_back(resolve({column, row}, {}));
        for (std::int64_t row = 0; row < extent.rows; ++row)
            for (std::int64_t column = 0; column < extent.columns; ++column)
                tiles_.set(layer_, {column, row}, derived[index({column, row})]);
    }

    bool TerrainMap::contains(const Maps::Point point) const {
        return point.column >= 0 && point.row >= 0 && point.column < tiles_.extent().columns && point.row < tiles_.extent().rows;
    }

    std::size_t TerrainMap::index(const Maps::Point point) const {
        if (!contains(point))
            throw CE::Exceptions::bad_request(CE_HERE, "Terrain cell coordinates are out of bounds");
        return static_cast<std::size_t>(point.row) * tiles_.extent().columns + static_cast<std::size_t>(point.column);
    }

    TerrainLabel TerrainMap::label(const Maps::Point point) const {
        return labels_[index(point)];
    }

    TerrainMap::~TerrainMap() = default;

    const Maps::Map& TerrainMap::tiles() const {
        return tiles_;
    }

    Maps::TileId TerrainMap::resolve(const Maps::Point point, const std::optional<Change> change) const {
        const auto at = [&](const Maps::Point site) { return change && site == change->point ? change->label : label(site); };
        return derive(point, rule_, target_to_id_, at, [&](const Maps::Point site) { return contains(site); });
    }

    std::vector<Maps::Point> TerrainMap::edit(const Maps::Point point, const TerrainLabel label) {
        const auto edited = index(point); // Reject extreme signed inputs BEFORE neighbor arithmetic.
        if (labels_[edited] == label)
            return {};
        std::vector<Maps::Point> affected;
        affected.reserve(5);
        affected.push_back(point);
        for (const auto offset : offsets) {
            const Maps::Point neighbor{point.column + offset.column, point.row + offset.row};
            if (contains(neighbor) && std::ranges::find(affected, neighbor) == affected.end())
                affected.push_back(neighbor);
        }
        std::array<Maps::TileId, 5> derived{};
        for (std::size_t i = 0; i < affected.size(); ++i)
            derived[i] = resolve(affected[i], Change{point, label});
        // All owned results and return storage exist. Private fixed map/layer
        // invariants make these assignments nonallocating and valid; no caller
        // can observe this commit midway under the serialized-owner contract.
        labels_[edited] = label;
        for (std::size_t i = 0; i < affected.size(); ++i)
            tiles_.set(layer_, affected[i], derived[i]);
        return affected;
    }

    std::vector<Point> TerrainMap::edit_batch(const std::span<const TerrainEdit> requests, const TerrainBatchLimits limits) {
        if (requests.size() > limits.requests)
            throw CE::Exceptions::bad_request(CE_HERE, "Terrain batch request cap exhausted");
        Work work{limits.work};
        // Validate the COMPLETE input before building any proposal. Last-wins
        // never hides invalid earlier points. Every representable label is valid.
        for (const auto& request : requests) {
            work.take();
            static_cast<void>(index(request.point));
        }
        std::map<std::size_t, Change> proposed;
        std::vector<std::size_t> order;
        for (const auto& request : requests) {
            const auto cell = index(request.point);
            const auto [entry, inserted] = proposed.try_emplace(cell, Change{request.point, request.label});
            if (inserted)
                order.push_back(cell);
            else
                entry->second.label = request.label;
        }
        std::erase_if(proposed, [&](const auto& entry) { return entry.second.label == labels_[entry.first]; });

        std::set<std::size_t> seen;
        std::vector<Point> affected;
        const auto visit = [&](const Point point) {
            work.take(); // Includes bounded exterior candidates, never unbounded coordinate arithmetic.
            if (!contains(point))
                return;
            const auto cell = index(point);
            if (seen.contains(cell))
                return;
            if (affected.size() == limits.affected_cells)
                throw CE::Exceptions::bad_request(CE_HERE, "Terrain batch affected-cell cap exhausted");
            seen.insert(cell);
            affected.push_back(point);
        };
        for (const auto cell : order) {
            const auto entry = proposed.find(cell);
            if (entry == proposed.end())
                continue; // Final no-op/cancellation; it contributes no invalidation.
            const auto point = entry->second.point;
            visit(point);
            for (const auto offset : offsets)
                visit({point.column + offset.column, point.row + offset.row});
        }
        const auto at = [&](const Point point) {
            const auto cell = index(point);
            const auto entry = proposed.find(cell);
            return entry == proposed.end() ? labels_[cell] : entry->second.label;
        };
        std::vector<TileId> derived;
        for (const auto point : affected)
            derived.push_back(derive(point, rule_, target_to_id_, at, [&](const Point site) { return contains(site); }, &work));

        // No allocation or refusal remains: all indices/layer are private fixed
        // invariants, and every proposed label/visual/return value is owned.
        for (const auto& [cell, change] : proposed)
            labels_[cell] = change.label;
        for (std::size_t i = 0; i < affected.size(); ++i)
            tiles_.set(layer_, affected[i], derived[i]);
        return affected;
    }

}
