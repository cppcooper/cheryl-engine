#pragma once

#include "cave.h"

namespace ProcGen {
    inline constexpr std::uint32_t dungeon_max_rooms = 64, dungeon_max_attempts = 8192;
    inline constexpr CaveBudgets dungeon_hard_limits{65'536, 4 * 1024 * 1024, 100'000'000};
    inline constexpr std::uint64_t dungeon_placement_stage = 0x64756e67656f6e50, dungeon_corridor_stage = 0x64756e67656f6e43;
    struct DungeonConfiguration {
        std::uint32_t rooms = 8;
        std::array<Index, 2> minimum{2, 2}, maximum{4, 4}; // inclusive widths/heights, positive
        Index spacing = 1;                                 // >=this many blank cells in at least one axis between rectangles
        Index margin = 1;                                  // >=1; applies to rooms AND center-to-center corridors
        std::uint32_t attempts = 256;                      // total candidates, including fit/overlap rejections; zero is valid
        bool operator==(const DungeonConfiguration&) const = default;
    };
    enum class DungeonAlgorithm : std::uint32_t { rooms_and_chain = 1 };
    struct DungeonReplay {
        Domain domain;
        StreamKey random;
        DungeonConfiguration requested;
        CaveBudgets budgets;
        DungeonAlgorithm algorithm = DungeonAlgorithm::rooms_and_chain;
        std::uint32_t version = 1;
        bool operator==(const DungeonReplay&) const = default;
    };
    struct DungeonRoom {
        Index id;
        std::array<Index, 2> lower, upper; // half-open [lower.x,upper.x) x [lower.y,upper.y)
        bool operator==(const DungeonRoom&) const = default;
    };
    struct DungeonConnection {
        Index from, to;   // room IDs i-1,i; logical chain, not every geometric adjacency
        Index first_axis; // 0=x then y, 1=y then x
        Index length;     // cardinal edges between lower-center cells
        Index carved;     // formerly solid cells changed by THIS corridor, intersections counted once
        bool operator==(const DungeonConnection&) const = default;
    };
    enum class DungeonAchievement { complete, attempt_limit };
    struct DungeonMeasurements {
        Index requested = 0, achieved = 0, attempts = 0, fit_rejections = 0, overlap_rejections = 0;
        Index room_floor = 0, corridor_floor = 0, floor = 0, solid = 0;
        bool connected = false; // exactly one NONEMPTY cardinal floor component
        DungeonAchievement achievement = DungeonAchievement::complete;
        bool operator==(const DungeonMeasurements&) const = default;
    };
    struct DungeonRequirements {
        Index cells, storage_bytes, minimum_work;
    };

    // Complete independently owned rooms, logical connections and binary0/1
    // occupancy; index=x+width*y. Copies deep-copy each vector. Spans expire on
    // destruction/move/assignment; moved-from supports destruction/assignment only.
    // IDs are local to this generated result, not cross-seed persistent IDs.
    class Dungeon {
        DungeonReplay replay_;
        std::vector<DungeonRoom> rooms_;
        std::vector<DungeonConnection> connections_;
        std::vector<std::uint8_t> occupancy_;
        DungeonMeasurements measured_;
        Usage usage_;
        Dungeon(
            DungeonReplay replay,
            std::vector<DungeonRoom> rooms,
            std::vector<DungeonConnection> connections,
            std::vector<std::uint8_t> occupancy,
            DungeonMeasurements measured,
            Usage usage
        );
        friend Result<Dungeon> replay_dungeon(const DungeonReplay& replay);
    public:
        [[nodiscard]] const DungeonReplay& descriptor() const { return replay_; }
        [[nodiscard]] const DungeonConfiguration& effective() const { return replay_.requested; }
        [[nodiscard]] std::span<const DungeonRoom> rooms() const { return rooms_; }
        [[nodiscard]] std::span<const DungeonConnection> connections() const { return connections_; }
        [[nodiscard]] std::span<const std::uint8_t> occupancy() const { return occupancy_; }
        [[nodiscard]] DungeonMeasurements measurements() const { return measured_; }
        [[nodiscard]] Usage usage() const { return usage_; }
    };
    // Peak requested payload4096+10*C+80*requested_rooms: output/visited bytes,
    // C u64 flood queue, capacity R each 40-byte rooms/connections; not RSS.
    // Minimum4*C covers buffer initialization and final validation scan only;
    // runtime also charges every draw/rejection, pair test, carve and BFS operation.
    [[nodiscard]] Result<DungeonRequirements> dungeon_requirements(const Domain& domain, DungeonConfiguration config);
    // V1: derive base=derive_stream_state(random), then independent streams
    // for_stage({base,dungeon_placement_stage,0}) and corridor_stage similarly.
    // Each candidate draws width,height then x,y if size fits. No clamping.
    // Oversize candidates reject; minimum size unable to fit is contradiction.
    // Unmet target after attempts is explicit partial-achievement SUCCESS, not
    // proof of impossibility. A budget refusal publishes no partial Dungeon.
    // Rooms do not overlap, spacing applies before corridors. Corridors may cross
    // rooms/each other and create cycles/adjoining rooms beyond the chain graph.
    // Centers=lower+(upper-lower-1)/2. Nonempty floor connectivity is validated.
    // Margin keeps perimeter solid, including empty/tiny domains. No invented
    // room for zero achieved; connected=false. Budgets affect admission only;
    // attempts is content configuration. Allocation/programming exceptions propagate.
    [[nodiscard]] Result<Dungeon> replay_dungeon(const DungeonReplay& replay);
    [[nodiscard]] Result<Dungeon>
    generate_dungeon(std::span<const Index> extents, StreamKey random, DungeonConfiguration config, CaveBudgets budget);
}
