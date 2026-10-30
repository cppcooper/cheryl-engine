#pragma once

#include <cheryl/export.h>

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
/** One row-major grid cell and positive millisecond duration in validated clip metadata. */
    struct TimedFrame {
        CE::Assets::Grids::CellIndex cell{}; ///< Zero-based row-major grid cell; validated callers keep it inside the owning grid.
        std::chrono::milliseconds duration{}; ///< Frame duration in milliseconds; validated timelines require strictly positive values.
    };
}

namespace CE::Assets::Sprites {
    /** Concrete ordered sprite animation, optionally qualified by facing; owns metadata, no resource or clock. */
    struct Clip {
        std::string name; ///< Owned clip lookup name; sprite identity also includes optional facing.
        std::optional<std::string> facing; ///< Optional exact facing qualifier; absent denotes an unqualified sprite clip.
        std::string description; ///< Owned human-readable metadata, not an executable behavior or lookup key.
        std::vector<CE::Assets::Animation::TimedFrame> frames; ///< Frames in playback order; valid clips are nonempty with positive durations.
        bool loop{}; ///< True repeats the timeline; false holds the last frame/cell after completion.
    };
}

namespace CE::Assets::Tilesets {
    /** Timeline replacing one original tileset target; frame cells are not recursively animated. */
    struct Clip {
        std::string name; ///< Owned descriptive clip name; the target cell selects the replacement timeline.
        std::string description; ///< Owned human-readable metadata, not an executable behavior or lookup key.
        CE::Assets::Grids::CellIndex target{}; ///< Original static grid cell whose selection this timeline replaces.
        std::vector<CE::Assets::Animation::TimedFrame> frames; ///< Frames in playback order; valid clips are nonempty with positive durations.
        bool loop{}; ///< True repeats the timeline; false holds the last frame/cell after completion.

        // Return a cell index for caller-owned simulation time.
        // Exact duration boundaries enter the next frame; loops wrap and other clips
        // hold their last cell. Negative time, empty/nonpositive or overflowing
        // timelines throw invalid_args. Cell/grid bounds remain the caller's contract.
        [[nodiscard]] CE_API CE::Assets::Grids::CellIndex cell_at(std::chrono::milliseconds elapsed) const;
    };
}

namespace CE::Assets::Sprites {
/** Reusable action row/column recipe expanded with each named facing; aggregate assignment does not validate bounds. */
    struct ProfileClip {
        std::string description; ///< Owned human-readable metadata, not an executable behavior or lookup key.
        std::size_t row_offset{}; ///< Action row offset added to a profile facing offset.
        std::vector<std::size_t> columns; ///< Ordered zero-based grid columns for the expanded clip.
        std::chrono::milliseconds frame_duration{}; ///< Positive common millisecond duration assigned to each expanded frame.
        bool loop{}; ///< True repeats the timeline; false holds the last frame/cell after completion.
    };

    /** Owned facing/action templates expanded by manifest parsing into concrete sprite clips. */
    struct Profile {
        std::string description; ///< Owned human-readable metadata, not an executable behavior or lookup key.
        std::unordered_map<std::string, std::size_t> facings; ///< Named zero-based facing row offsets added to each action row_offset.
        std::unordered_map<std::string, ProfileClip> clips; ///< Named action recipes used to construct concrete clips.
    };
}

/**
 * @fn CE::Assets::Grids::CellIndex CE::Assets::Tilesets::Clip::cell_at(std::chrono::milliseconds elapsed) const
 * @brief Resolve caller-owned simulation time into a replacement cell without mutating metadata.
 * @param elapsed Nonnegative time since the clip's application-defined origin, in milliseconds.
 * @return Frame cell; exact boundaries select the next frame, looping wraps, nonlooping holds the last.
 * @throws CE::Exceptions::invalid_args For negative time, empty/nonpositive frames or overflowing summed durations.
 * Cell/grid bounds remain the caller's contract; this operation creates no clock, resources or recursive animation lookup.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#tile-playback-and-rules)
 */
