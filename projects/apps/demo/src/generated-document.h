#pragma once

#include "world-document.h"
#include "cave.h"
#include <cheryl/core/resources/map-storage/cell-patch.h>

namespace WorldProof {
    enum class GeneratedReplace { DiscardCurrent, PreserveEdits };
    enum class CaveAxes { XRightYDown };
    struct OccupancyMeaning {
        std::string key;
        Tiles::TerrainLabel label;
        bool operator==(const OccupancyMeaning&) const = default;
    };
    struct CaveAdapter {
        std::uint32_t version = 1;
        CaveAxes axes = CaveAxes::XRightYDown;
        std::string id = "terrain:cave";
        std::array<std::string, 2> layers{"ground", "detail"};
        std::vector<Storage::Spawn> spawns;
        std::array<OccupancyMeaning, 2> occupancy{{{"terrain:one", {1}}, {"terrain:two", {2}}}};
        std::vector<std::optional<std::string>> initial_detail; // Empty means all null; otherwise exactly domain.cells().
        bool operator==(const CaveAdapter&) const = default;
    };
    struct CaveRecipe {
        ProcGen::CaveReplay cave;
        std::string family = "cave";
        CaveAdapter adapter{};
        bool operator==(const CaveRecipe&) const = default;
    };
    struct GeneratedLimits {
        std::size_t edits = 128;
        std::size_t witness_bytes = 1024 * 1024;
        std::size_t retained_bytes = 4 * 1024 * 1024;
        std::size_t work = 1024 * 1024;
        bool operator==(const GeneratedLimits&) const = default;
    };
    struct GeneratedBytes {
        std::size_t recipe{}, base{}, witness{}, edits{}, retained{}, full_snapshot{}, history{};
        std::size_t reserved_work{}, scan_passes = 64, copy_passes = 32;
        ProcGen::Index cave_storage{}, cave_work{};
        bool operator==(const GeneratedBytes&) const = default;
    };
    // Entirely owned observations, including full base and exact canonical witness.
    // Logical payload charges, not a compact format or allocator/RSS accounting.
    struct GeneratedDocument {
        CaveRecipe recipe;
        ProcGen::CaveConfiguration effective;
        GeneratedLimits limits;
        Storage::Snapshot base;
        Storage::CellPatches::Patch overlay;
        GeneratedBytes bytes;
    };
    [[nodiscard]] GeneratedDocument generate_document(const CaveRecipe& recipe, GeneratedLimits limits);
    [[nodiscard]] GeneratedDocument stage_generated_edits(const GeneratedDocument& previous, const Storage::Snapshot& final);
    // Proposed input changes require explicit discard, even if output coincides.
    // Budget-only changes may admit the identical semantic base. All base bytes
    // are checked before applying edits; inputs and older successful results survive refusal.
    [[nodiscard]] Storage::Snapshot replay_generated(const GeneratedDocument& retained, const CaveRecipe& proposal);
}
