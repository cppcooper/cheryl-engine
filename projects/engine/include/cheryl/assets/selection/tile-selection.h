#pragma once

#include <assets/definitions/tileset.h>

#include <cstdint>
#include <functional>
#include <variant>

namespace CE::Assets::Tilesets::Selection {
    enum class SiteKind { Cell, Edge, Corner };

    // Relative to the selected tile. Wang labels belong to shared edges/vertices;
    // bitmask labels belong to neighboring cells. The caller owns world coordinates.
    struct Site {
        SiteKind kind{};
        Direction direction{};

        [[nodiscard]] bool operator==(const Site&) const = default;
    };

    enum class SampleState { Known, Outside, Unknown };

    struct Sample {
        SampleState state{SampleState::Unknown};
        std::uint32_t terrain{}; // Known zero means empty; ignored for other states.
    };

    // Invoked synchronously for each required site once, in canonical direction order.
    // Keep the sampled world stable for the call. The selector never retains this callback.
    using Sampler = std::function<Sample(Site)>;

    enum class Fallback { Empty, Center, Unresolved };
    enum class DiagonalConnectivity { Independent, RequireCardinals };

    struct Options {
        std::uint32_t terrain{}; // Center terrain; zero never connects in a bitmask.
        std::uint64_t seed{}; // Caller-owned location/variant seed, independent of animation time.
        Fallback outside{Fallback::Empty};
        Fallback unknown{Fallback::Unresolved};
        DiagonalConnectivity diagonals{DiagonalConnectivity::RequireCardinals};
    };

    enum class Failure { MissingRule, IncompleteNeighborhood };
    using Result = std::variant<CE::Assets::Grids::CellIndex, Failure>;

    // CPU-only lookup: Wang candidates retain definition order and use finite positive
    // weights; bit_order owns bit positions. Unknown Wang IDs use the unknown policy.
    // Diagonal gating also samples adjacent cardinal cells absent from bit_order.
    // Missing/empty candidate lists return MissingRule; unresolved required samples
    // return IncompleteNeighborhood. Invalid options/rule metadata or an empty sampler
    // throw invalid_args; sampler exceptions propagate. No grid or animation lookup here.
    [[nodiscard]] Result select(const WangRule& rule, const Sampler& sampler, const Options& options);
    [[nodiscard]] Result select(const BitmaskRule& rule, const Sampler& sampler, const Options& options);
    [[nodiscard]] Result select(const Rule& rule, const Sampler& sampler, const Options& options);
}
