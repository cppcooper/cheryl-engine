#pragma once

#include "snapshot.h"

namespace MapFixture {
    // Literal expected data, independent of any document being verified.
    [[nodiscard]] inline Snapshot fixed_fixture() {
        return {
            .id = "fixture:two-layers", .width = 3, .height = 2,
            .layers = {
                {.id = "ground", .cells = {"fixture:grass", "fixture:stone", "fixture:grass",
                                          "fixture:grass", "fixture:grass", "fixture:stone"}},
                {.id = "overlay", .cells = {std::nullopt, "fixture:wall", std::nullopt,
                                           std::nullopt, std::nullopt, "fixture:wall"}}
            },
            .spawns = {{.name = "entry", .layer = "ground", .x = 1, .y = 1}}
        };
    }
}
