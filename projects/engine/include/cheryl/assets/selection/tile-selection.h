#pragma once

#include <assets/definitions/tileset.h>

#include <cstdint>
#include <functional>
#include <variant>

namespace CE::Assets {
    enum class TerrainSiteKind { Cell, Edge, Corner };

    // Relative to the selected tile. Wang labels belong to shared edges/vertices;
    // bitmask labels belong to neighboring cells. The caller owns world coordinates.
    struct TerrainSite {
        TerrainSiteKind kind{};
        Direction direction{};

        [[nodiscard]] bool operator==(const TerrainSite&) const = default;
    };

    enum class TerrainSampleState { Known, Outside, Unknown };

    struct TerrainSample {
        TerrainSampleState state{TerrainSampleState::Unknown};
        std::uint32_t terrain{}; // Known zero means empty; ignored for other states.
    };

    // Invoked synchronously for each required site once, in canonical direction order.
    // Keep the sampled world stable for the call. The selector never retains this callback.
    using TerrainSampler = std::function<TerrainSample(TerrainSite)>;

    enum class TerrainFallback { Empty, Center, Unresolved };
    enum class DiagonalConnectivity { Independent, RequireCardinals };

    struct TileSelectionOptions {
        std::uint32_t terrain{}; // Center terrain; zero never connects in a bitmask.
        std::uint64_t seed{}; // Caller-owned location/variant seed, independent of animation time.
        TerrainFallback outside{TerrainFallback::Empty};
        TerrainFallback unknown{TerrainFallback::Unresolved};
        DiagonalConnectivity diagonals{DiagonalConnectivity::RequireCardinals};
    };

    enum class TileSelectionFailure { MissingRule, IncompleteNeighborhood };
    using TileSelectionResult = std::variant<CellIndex, TileSelectionFailure>;

    // CPU-only lookup: Wang candidates retain definition order and use finite positive
    // weights; bit_order owns bit positions. Unknown Wang IDs use the unknown policy.
    // Diagonal gating also samples adjacent cardinal cells absent from bit_order.
    // Missing/empty candidate lists return MissingRule; unresolved required samples
    // return IncompleteNeighborhood. Invalid options/rule metadata or an empty sampler
    // throw invalid_args; sampler exceptions propagate. No grid or animation lookup here.
    [[nodiscard]] TileSelectionResult
    select_tile(const WangAutotileDefinition& rule, const TerrainSampler& sampler, const TileSelectionOptions& options);
    [[nodiscard]] TileSelectionResult
    select_tile(const BitmaskAutotileDefinition& rule, const TerrainSampler& sampler, const TileSelectionOptions& options);
    [[nodiscard]] TileSelectionResult
    select_tile(const AutotileDefinition& rule, const TerrainSampler& sampler, const TileSelectionOptions& options);
}
