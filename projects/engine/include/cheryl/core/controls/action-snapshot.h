#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

namespace CE::Input {
    using ObservationClock = std::chrono::steady_clock;
    using ObservationDuration = std::chrono::duration<double>; // Observation-clock seconds.
    enum class AxisKind { Absolute, Relative };

} // namespace CE::Input

namespace CE::Input::Actions {
    /** Game-defined semantic identifier. A game's enums can be converted explicitly at its boundary. */
    struct Id {
        std::uint32_t value;
        constexpr explicit Id(std::uint32_t id)
        : value(id) {}
        constexpr bool operator==(const Id&) const = default;
    };
} // namespace CE::Input::Actions

template <> struct std::hash<CE::Input::Actions::Id> {
    std::size_t operator()(const CE::Input::Actions::Id id) const noexcept { return std::hash<std::uint32_t>{}(id.value); }
};

namespace CE::Input::Actions {
    /** State at one completed input poll; both edges may be true for a press and release in that poll. */
    struct ButtonPollState {
        bool current = false;
        bool previous = false;
        bool pressed_this_poll = false;
        bool released_this_poll = false;
        std::uint64_t press_count = 0;
        std::uint64_t release_count = 0;
        std::optional<CE::Input::ObservationClock::time_point> hold_started_at;
        std::vector<CE::Input::ObservationDuration> completed_holds;

        [[nodiscard]] bool held() const { return current; }
        [[nodiscard]] bool pressed() const { return pressed_this_poll; }
        [[nodiscard]] bool released() const { return released_this_poll; }
    };

    struct AxisPollState {
        float current = 0.0f;
        float previous = 0.0f;
        CE::Input::AxisKind kind = CE::Input::AxisKind::Absolute;

        [[nodiscard]] float delta() const { return kind == CE::Input::AxisKind::Relative ? current : current - previous; }
    };

    /** The complete set of semantic actions from one poll. Copies/handles stay stable after later polls. */
    class Snapshot {
        friend class Bindings;
        std::uint64_t poll_ = 0;
        std::chrono::steady_clock::time_point observed_at_ = std::chrono::steady_clock::now();
        std::unordered_map<Id, ButtonPollState> buttons_;
        std::unordered_map<Id, AxisPollState> axes_;

    public:
        [[nodiscard]] std::uint64_t poll() const { return poll_; }
        [[nodiscard]] std::chrono::steady_clock::time_point observed_at() const { return observed_at_; }
        [[nodiscard]] bool has_changes() const {
            for (const auto& [id, state] : buttons_)
                if (state.pressed() || state.released())
                    return true;
            for (const auto& [id, state] : axes_)
                if (state.delta() != 0.0f)
                    return true;
            return false;
        }
        // Owned state copies; missing actions return inactive/zero defaults.
        [[nodiscard]] ButtonPollState button(Id action) const {
            const auto found = buttons_.find(action);
            return found == buttons_.end() ? ButtonPollState{} : found->second;
        }
        [[nodiscard]] AxisPollState axis(Id action) const {
            const auto found = axes_.find(action);
            return found == axes_.end() ? AxisPollState{} : found->second;
        }
        [[nodiscard]] bool has_axis(Id action) const { return axes_.contains(action); }
    };
} // namespace CE::Input::Actions
