#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

namespace CE::Input {
    using InputClock = std::chrono::steady_clock;
    using InputDuration = std::chrono::duration<double>;
    enum class AxisKind { Absolute, Relative };

    /** Game-defined semantic identifier. A game's enums can be converted explicitly at its boundary. */
    struct ActionId {
        std::uint32_t value;
        constexpr explicit ActionId(std::uint32_t id)
        : value(id) {}
        constexpr bool operator==(const ActionId&) const = default;
    };
} // namespace CE::Input

template <> struct std::hash<CE::Input::ActionId> {
    std::size_t operator()(const CE::Input::ActionId id) const noexcept { return std::hash<std::uint32_t>{}(id.value); }
};

namespace CE::Input {
    /** State at one completed input poll; both edges may be true for a press and release in that poll. */
    struct ButtonActionState {
        bool current = false;
        bool previous = false;
        bool pressed_this_poll = false;
        bool released_this_poll = false;
        std::uint64_t press_count = 0;
        std::uint64_t release_count = 0;
        std::optional<InputClock::time_point> hold_started_at;
        std::vector<InputDuration> completed_holds;

        [[nodiscard]] bool held() const { return current; }
        [[nodiscard]] bool pressed() const { return pressed_this_poll; }
        [[nodiscard]] bool released() const { return released_this_poll; }
    };

    struct AxisActionState {
        float current = 0.0f;
        float previous = 0.0f;
        AxisKind kind = AxisKind::Absolute;

        [[nodiscard]] float delta() const { return kind == AxisKind::Relative ? current : current - previous; }
    };

    /** The complete set of semantic actions from one poll. Copies/handles stay stable after later polls. */
    class ActionSnapshot {
        friend class InputBindings;
        std::uint64_t poll_ = 0;
        std::chrono::steady_clock::time_point observed_at_ = std::chrono::steady_clock::now();
        std::unordered_map<ActionId, ButtonActionState> buttons_;
        std::unordered_map<ActionId, AxisActionState> axes_;

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
        [[nodiscard]] ButtonActionState button(ActionId action) const {
            const auto found = buttons_.find(action);
            return found == buttons_.end() ? ButtonActionState{} : found->second;
        }
        [[nodiscard]] AxisActionState axis(ActionId action) const {
            const auto found = axes_.find(action);
            return found == axes_.end() ? AxisActionState{} : found->second;
        }
        [[nodiscard]] bool has_axis(ActionId action) const { return axes_.contains(action); }
    };
} // namespace CE::Input
