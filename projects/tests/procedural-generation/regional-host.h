#pragma once
#include "regional.h"
#include <limits>
#include <stdexcept>
#include <utility>

// Tiny APPLICATION example shared by the CPU caller/check. It selects wet alerts,
// saves its marker beside the weather image, and adopts them together. This is
// not a weather API/event framework or an external exactly-once guarantee.
namespace ProcGenRegionalHost {
    struct Marker {
        ProcGen::Index last_applied_tick, wet_alerts;
    };
    struct Save {
        ProcGen::RegionalCheckpoint checkpoint;
        Marker marker;
    };
    class Application {
        ProcGen::RegionalState state_;
        Marker marker_;
        Application(ProcGen::RegionalState state, Marker marker)
        : state_(std::move(state)), marker_(marker) {}
    public:
        explicit Application(ProcGen::RegionalState state)
        : state_(std::move(state)), marker_{state_.weather().tick(), 0} {}
        [[nodiscard]] const ProcGen::RegionalState& state() const { return state_; }
        [[nodiscard]] Marker marker() const { return marker_; }
        void adopt(ProcGen::RegionalAdvance next) {
            if (next.report.from_tick != state_.weather().tick() || next.report.to_tick != next.state.weather().tick() ||
                marker_.last_applied_tick != next.report.from_tick)
                throw std::invalid_argument("host interval/state marker mismatch");
            auto marker = marker_;
            marker.last_applied_tick = next.report.to_tick;
            if (next.report.water.infiltration_m3 > 1) {
                if (marker.wet_alerts == std::numeric_limits<ProcGen::Index>::max())
                    throw std::overflow_error("host alert count");
                ++marker.wet_alerts;
            }
            Application candidate(std::move(next.state), marker);
            *this = std::move(candidate);
        }
        [[nodiscard]] Save save(ProcGen::RegionalBudgets budgets) const {
            return {ProcGen::capture_regional(state_, budgets).value(), marker_};
        }
        [[nodiscard]] static Application
        restore(const Save& save, const ProcGen::ClimateBaseline& expected, ProcGen::RegionalBudgets budgets) {
            return restore(save.checkpoint, save.marker, expected, budgets);
        }
        [[nodiscard]] static Application restore(
            const ProcGen::RegionalCheckpoint& image,
            Marker marker,
            const ProcGen::ClimateBaseline& expected,
            ProcGen::RegionalBudgets budgets
        ) {
            auto state = ProcGen::resume_regional(image, expected, budgets);
            if (!state || marker.last_applied_tick != state->weather().tick())
                throw std::invalid_argument("host restore state/marker mismatch");
            return Application(std::move(*state), marker);
        }
        // Repeated observations/old impact reports cannot apply another effect.
        // Future reports must enter through adopt, together with accepted state.
        [[nodiscard]] bool ignore_old(const ProcGen::RegionalReport& report) const {
            if (report.to_tick > marker_.last_applied_tick)
                throw std::invalid_argument("future impact requires coherent adoption");
            return true;
        }
    };
}
