#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace CE::Maps {
    /** An owned ordered layer in the cheryl.map-fixture v1 CPU representation.
     * Logical tile keys are independent of installed assets. Local names match
     * [a-z0-9][a-z0-9_-]*; qualified map/tile keys add a namespace (also allowing
     * dots) and one colon. Keys are case-sensitive and at most 128 ASCII bytes.
     */
    struct Layer {
        std::string id; ///< Unique map-local layer name.
        std::vector<std::optional<std::string>> cells; ///< width * height keys in row order; nullopt means empty.

        [[nodiscard]] bool operator==(const Layer&) const = default;
    };

    /** An owned named cell reference; creates no entity and imposes no walkability rule. */
    struct Spawn {
        std::string name; ///< Unique map-local spawn name.
        std::string layer; ///< Exact name of an existing layer.
        std::uint32_t x = 0; ///< Cell column in [0, width).
        std::uint32_t y = 0; ///< Cell row in [0, height).

        [[nodiscard]] bool operator==(const Spawn&) const = default;
    };

    /** Independent owned finite-map values; equality compares every field in order.
     * V1 uses zero-origin, +x right, +y down; each layer uses y * width + x.
     * These stored cell coordinates do not select application world coordinates.
     * Aggregate construction does not validate. parse() and encode() validate
     * completely, including the operational limits declared in codec.h.
     */
    struct Snapshot {
        std::string id; ///< Qualified content key, independent of file path or runtime address.
        std::uint32_t width = 0; ///< Positive column count when validated; default zero is invalid.
        std::uint32_t height = 0; ///< Positive row count when validated; default zero is invalid.
        std::vector<Layer> layers; ///< Nonempty authored order; all layers share dimensions.
        std::vector<Spawn> spawns; ///< Optional named references, retaining authored order.

        [[nodiscard]] bool operator==(const Snapshot&) const = default;
    };
}
