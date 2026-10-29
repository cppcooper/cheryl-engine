#pragma once

#include <cheryl/export.h>

#include <chrono>
#include <optional>

namespace CE::Runtime {
    /** Render-start pacing, independent of simulation timing and input polling. */
    struct FramePacingOptions {
        bool enabled = false;     ///< Enable the software cap; disabled preserves existing render scheduling.
        double target_fps = 60.0; ///< Finite positive target, retained even while disabled. Vsync/work may produce a lower rate.
    };

    /** CPU-only monotonic scheduling. The owner checks due() before rendering and
     * records the actual frame start; missed deadlines never accumulate catch-up frames.
     * This value is unsynchronized and neither sleeps nor calls native adapters.
     */
    class FramePacer final {
    public:
        using Clock = std::chrono::steady_clock;

    private:
        FramePacingOptions options_;
        Clock::duration interval_{};
        std::optional<Clock::time_point> next_;

    public:
        /** Reject nonpositive/nonfinite targets and periods outside the clock's
         * duration range, including when disabled. Round positive periods up to
         * a clock tick so quantization cannot exceed the requested rate.
         */
        CE_API explicit FramePacer(FramePacingOptions options = {});
        [[nodiscard]] const FramePacingOptions& options() const noexcept { return options_; }
        [[nodiscard]] CE_API bool due(Clock::time_point now) const noexcept;
        // First frame is immediately eligible. Disabled/unstarted pacers have no
        // wake deadline and return max(); check due() before deciding to wait.
        [[nodiscard]] CE_API Clock::time_point next_frame_at() const noexcept;
        // Call on the rendering owner immediately before eligible native work.
        // Rebase on this actual start, accounting for render/present work in the
        // interval. Saturate an unrepresentable future deadline at max().
        CE_API void frame_started(Clock::time_point now) noexcept;
    };
}
