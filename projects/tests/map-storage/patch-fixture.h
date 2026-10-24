#pragma once

#include "cell-patch.h"

namespace PatchFixture {
    inline CE::Maps::Snapshot base() {
        return {"garden:north", 2, 3,
                {{"soil", {"garden:earth", "garden:stone", "garden:earth",
                           "garden:water", "garden:earth", "garden:earth"}},
                 {"markers", {std::nullopt, "garden:flag", std::nullopt,
                              std::nullopt, "garden:tree", std::nullopt}}},
                {{"gate", "soil", 0, 2}, {"lookout", "markers", 1, 0}}};
    }

    // Independent complete literal: never derived from apply() or the loaded file.
    inline CE::Maps::Snapshot expected() {
        return {"garden:north", 2, 3,
                {{"soil", {"garden:earth", "garden:sand", "garden:earth",
                           "garden:water", "garden:earth", "garden:earth"}},
                 {"markers", {std::nullopt, "garden:flag", "garden:flower",
                              std::nullopt, "garden:tree", std::nullopt}}},
                {{"gate", "soil", 0, 2}, {"lookout", "markers", 1, 0}}};
    }

    inline CE::Maps::Snapshot predecessor() {
        return {"garden:predecessor", 2, 3,
                {{"soil", {"garden:clay", "garden:clay", "garden:clay",
                           "garden:clay", "garden:clay", "garden:clay"}},
                 {"markers", {"garden:flag", std::nullopt, std::nullopt,
                              std::nullopt, std::nullopt, "garden:tree"}}},
                {{"gate", "soil", 1, 2}, {"lookout", "markers", 0, 0}}};
    }

    inline CellPatches::Patch patch() {
        return {CellPatches::capture(base()),
                {{"soil", 1, 0, "garden:sand"}, {"markers", 0, 1, "garden:flower"}}};
    }
}
