#pragma once

#include <assets/definitions/grid.h>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace CE::Assets::Animation {
    // Owned CPU metadata; validated clips have in-grid cells and positive millisecond
    // durations. Aggregate construction alone does not validate those requirements.
    struct TimedFrame {
        CE::Assets::Grids::CellIndex cell{};
        std::chrono::milliseconds duration{};
    };
}

namespace CE::Assets::Sprites {
    struct Clip {
        std::string name;
        std::optional<std::string> facing;
        std::string description;
        std::vector<CE::Assets::Animation::TimedFrame> frames;
        bool loop{};
    };
}

namespace CE::Assets::Tilesets {
    struct Clip {
        std::string name;
        std::string description;
        CE::Assets::Grids::CellIndex target{};
        std::vector<CE::Assets::Animation::TimedFrame> frames;
        bool loop{};

        // Return a cell index for caller-owned simulation time.
        // Exact duration boundaries enter the next frame; loops wrap and other clips
        // hold their last cell. Negative time, empty/nonpositive or overflowing
        // timelines throw invalid_args. Cell/grid bounds remain the caller's contract.
        [[nodiscard]] CE::Assets::Grids::CellIndex cell_at(std::chrono::milliseconds elapsed) const;
    };
}

namespace CE::Assets::Sprites {
    struct ProfileClip {
        std::string description;
        std::size_t row_offset{};
        std::vector<std::size_t> columns;
        std::chrono::milliseconds frame_duration{};
        bool loop{};
    };

    struct Profile {
        std::string description;
        std::unordered_map<std::string, std::size_t> facings;
        std::unordered_map<std::string, ProfileClip> clips;
    };
}
