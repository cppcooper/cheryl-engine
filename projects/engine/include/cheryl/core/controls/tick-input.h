#pragma once

#include <cheryl/export.h>

#include "action-snapshot.h"
#include "action-context.h"
#include "input-record.h"

#include <memory>
#include <ranges>
#include <span>
#include <vector>

namespace CE::Input {
    namespace Actions {
        class View;
        /** Button activity since the previous simulation consumption.
     * Durations use poll observation times, not unreported hardware times.
     */
        struct ButtonTickState {
            bool current = false; ///< Final held state at the consumption boundary.
            bool previous = false; ///< Held state at the preceding consumption boundary.
            bool pressed_this_tick = false; ///< At least one press in consumed polls.
            bool released_this_tick = false; ///< At least one release in consumed polls.
            std::uint64_t press_count = 0; ///< Sum of observed press transitions in the batch.
            std::uint64_t release_count = 0; ///< Sum of observed release transitions in the batch.
            CE::Input::ObservationDuration held_duration{}; ///< Active hold age in observation seconds, or zero after release.
            CE::Input::ObservationDuration down_duration{}; ///< Observed down-time within this consumption interval, in seconds.
            std::vector<CE::Input::ObservationDuration> completed_holds; ///< Owned full durations of each hold released in this batch.

            [[nodiscard]] bool held() const { return current; }
            [[nodiscard]] bool pressed() const { return pressed_this_tick; }
            [[nodiscard]] bool released() const { return released_this_tick; }
        };

        /** Absolute axes retain their final value; relative axes sum this batch's deltas. */
        struct AxisTickState {
            float current = 0.0f; ///< Final absolute value or sum of this tick's relative deltas.
            float previous = 0.0f; ///< Baseline absolute value; zero for a Relative tick.
            CE::Input::AxisKind kind = CE::Input::AxisKind::Absolute; ///< Reported kind retained across absent/unbound samples.

            [[nodiscard]] float delta() const { return kind == CE::Input::AxisKind::Relative ? current : current - previous; }
        };

    }

    /** Immutable State and ordered records for one independently scheduled update.
     * The baseline is the last consumed sample. New polls are consumed together
     * in publication order, regardless of how many transitions they contain.
     * Owns sample handles/record storage. Read-only queries may cross threads;
     * spans/filter views borrow this value and cannot survive its move/destruction.
     */
    class Tick {
        friend class Accumulator;

    public:
        // Baseline must be nonnull; polls must be nonnull, strictly newer by ID,
        // nondecreasing by observation time, and no later than until. since must be
        // at/after the baseline and until at/after since; invalid input throws.
        // For manual consumers, the interval defaults to the sample timestamps.
        CE_API Tick(std::shared_ptr<const Actions::Snapshot> previous, std::vector<std::shared_ptr<const Actions::Snapshot>> polls);
        CE_API Tick(
            std::shared_ptr<const Actions::Snapshot> previous,
            std::vector<std::shared_ptr<const Actions::Snapshot>> polls,
            ObservationClock::time_point since,
            ObservationClock::time_point until
        );

        // Owned state copies. axis() rejects a kind change within this poll batch;
        // changing kind between consumptions is supported. Durations remain seconds.
        [[nodiscard]] CE_API Actions::ButtonTickState button(Actions::Id action) const;
        [[nodiscard]] CE_API Actions::AxisTickState axis(Actions::Id action) const;
        // The view copies namespace identity and borrows this Tick, not the lease.
        [[nodiscard]] CE_API Actions::View actions(const Actions::Context& context) const;
        [[nodiscard]] CE_API Actions::View actions(Actions::ContextId context) const;
        [[nodiscard]] std::span<const std::shared_ptr<const Actions::Snapshot>> polls() const { return polls_; }
        // Retained final sample, or the baseline if this batch is empty.
        [[nodiscard]] CE_API std::shared_ptr<const Actions::Snapshot> latest_poll() const;
        [[nodiscard]] ObservationDuration elapsed() const { return until_ - since_; }
        // Shared, non-destructive view for every consumer in this simulation update.
        [[nodiscard]] std::span<const Events::Record> records() const { return records_; }
        // Routed text and editing controls remain interleaved in observation order.
        [[nodiscard]] auto records_for(const Routing::TargetId target, const std::optional<std::uint64_t> epoch = {}) const {
            return records() | std::views::filter([target, epoch](const Events::Record& record) {
                       return record.target == target && (!epoch || record.focus_epoch == *epoch);
                   });
        }
        [[nodiscard]] auto gameplay_events() const {
            return records() | std::views::filter([](const Events::Record& record) { return !record.is_text() && record.to_gameplay; });
        }

    private:
        friend class Actions::View;
        [[nodiscard]] CE_API Actions::ButtonTickState button(Actions::Id action, std::optional<Actions::ContextId> context) const;
        [[nodiscard]] CE_API Actions::AxisTickState axis(Actions::Id action, std::optional<Actions::ContextId> context) const;
        std::shared_ptr<const Actions::Snapshot> previous_;
        std::vector<std::shared_ptr<const Actions::Snapshot>> polls_;
        ObservationClock::time_point since_;
        ObservationClock::time_point until_;
        std::vector<Events::Record> records_;
    };

    namespace Actions {
        /** Local action queries for one namespace in a shared immutable Tick.
         * Copying retains the identity after context reset; the Tick must remain
         * alive and unmoved. Detachment never rewrites this historical view.
         */
        class View final {
            const Tick* tick_;
            ContextId context_;
            View(const Tick& tick, ContextId context)
            : tick_(&tick), context_(context) {}
            friend class CE::Input::Tick;

        public:
            [[nodiscard]] ButtonTickState button(Id action) const { return tick_->button(action, context_); }
            [[nodiscard]] AxisTickState axis(Id action) const { return tick_->axis(action, context_); }
        };
    }
}

/** @fn bool CE::Input::Actions::ButtonTickState::held() const
 * Return final held state.
 */

/** @fn bool CE::Input::Actions::ButtonTickState::pressed() const
 * Return whether at least one press occurred, even if released by this tick's end.
 */

/** @fn bool CE::Input::Actions::ButtonTickState::released() const
 * Return whether at least one release occurred, independently of final state.
 */

/** @fn float CE::Input::Actions::AxisTickState::delta() const
 * Return current for relative input, otherwise current-previous.
 */

/** @fn CE::Input::Tick::Tick(std::shared_ptr<const Actions::Snapshot> previous, std::vector<std::shared_ptr<const Actions::Snapshot>> polls)
 * Retain a nonnull baseline and strictly newer nonnull polls in nondecreasing observation order. Default interval runs from baseline time to the last poll, or baseline time when empty. Invalid inputs throw; no live adapter is borrowed.
 */

/** @fn CE::Input::Tick::Tick(std::shared_ptr<const Actions::Snapshot> previous, std::vector<std::shared_ptr<const Actions::Snapshot>> polls, ObservationClock::time_point since, ObservationClock::time_point until)
 * Retain an explicit monotonic observation interval. since must be at/after baseline time, until at/after since, and every newer poll must be observed by until. Invalid input throws before construction completes.
 */

/** @fn CE::Input::Actions::ButtonTickState CE::Input::Tick::button(Actions::Id action) const
 * Summarize direct-namespace levels, all edges/counts and observation durations into an owned value. Missing actions are inactive; completed-hold storage may allocate. No device polling occurs.
 */

/** @fn CE::Input::Actions::AxisTickState CE::Input::Tick::axis(Actions::Id action) const
 * Return final absolute state or summed relative deltas. A reported axis-kind change within this batch rejects; changes between consumptions are supported. Missing/unbound relative samples do not invent an Absolute transition.
 */

/** @fn CE::Input::Actions::View CE::Input::Tick::actions(const Actions::Context& context) const
 * Copy context.id() into a View borrowing this Tick, not the lease. Keep the Tick alive/unmoved; reset/destruction of the lease does not rewrite historical data.
 */

/** @fn CE::Input::Actions::View CE::Input::Tick::actions(Actions::ContextId context) const
 * Create a borrowed Tick view for a retained namespace identity. Missing namespaces return inactive/zero results; a live lease is unnecessary.
 */

/** @fn auto CE::Input::Tick::polls() const -> std::span<const std::shared_ptr<const Actions::Snapshot>>
 * Borrow the retained consumed sample-handle sequence; keep this Tick alive/unmoved, or copy handles for independent retention.
 */

/** @fn std::shared_ptr<const CE::Input::Actions::Snapshot> CE::Input::Tick::latest_poll() const
 * Return the retained last sample, or baseline when no polls were consumed.
 */

/** @fn CE::Input::ObservationDuration CE::Input::Tick::elapsed() const
 * Return until-since in observation seconds, independent of selected simulation delta/time scaling.
 */

/** @fn std::span<const CE::Input::Events::Record> CE::Input::Tick::records() const
 * Borrow the shared ordered physical/Text stream for this update. State-only construction has no records; complete-poll accumulation supplies them. The view cannot survive this Tick's move/destruction.
 */

/** @fn auto CE::Input::Tick::records_for(Routing::TargetId target, std::optional<std::uint64_t> epoch) const
 * Return a lazy borrowed filter preserving observation order for this target and, when specified, exact focus epoch. It selects matching records without consuming/modifying them; retain this unmoved Tick. Omitting epoch admits every collected epoch for that target.
 */

/** @fn auto CE::Input::Tick::gameplay_events() const
 * Return a borrowed ordered filter of non-Text records admitted to gameplay. Text is excluded even under PassThrough; retains no independent Tick ownership.
 */

/** @fn CE::Input::Actions::ButtonTickState CE::Input::Actions::View::button(Id action) const
 * Summarize a local button in the copied context namespace. Return owned state; keep the borrowed Tick alive/unmoved. Local IDs do not address direct bindings.
 */

/** @fn CE::Input::Actions::AxisTickState CE::Input::Actions::View::axis(Id action) const
 * Summarize a local axis using Tick kind/order rules. Missing context/actions yield zero defaults; the lease may already be detached.
 * @see [Views and shutdown](cheryl-guide:runtime/input-state-model#views-and-shutdown)
 */
