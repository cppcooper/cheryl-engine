#include <core/controls/input-bindings.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace CE::Input {
    BindingId InputBindings::bind_button(const DeviceBind control, const ActionId action) {
        return bind_button(InputChord{{control}}, action);
    }

    BindingId InputBindings::bind_button(InputChord chord, const ActionId action) {
        if (chord.required.empty())
            throw Exceptions::invalid_args(CE_HERE, "A button action needs at least one physical control");
        if (std::ranges::any_of(axis_bindings_, [action](const auto& binding) { return binding.action == action; }))
            throw Exceptions::invalid_args(CE_HERE, "An action cannot be bound as both a button and an axis");
        const BindingId id = next_binding_++;
        button_bindings_.push_back({id, std::move(chord), action});
        return id;
    }

    BindingId InputBindings::bind_axis(const DeviceBind axis, const ActionId action, const AxisOptions options) {
        return bind_axis(InputChord{}, axis, action, options);
    }

    BindingId InputBindings::bind_axis(InputChord modifiers, const DeviceBind axis, const ActionId action, const AxisOptions options) {
        if (!std::isfinite(options.scale) || !std::isfinite(options.dead_zone) || options.dead_zone < 0.0f || options.dead_zone >= 1.0f)
            throw Exceptions::invalid_args(CE_HERE, "An axis requires a finite scale and a dead zone in [0, 1)");
        if (std::ranges::any_of(button_bindings_, [action](const auto& binding) { return binding.action == action; }))
            throw Exceptions::invalid_args(CE_HERE, "An action cannot be bound as both a button and an axis");
        const BindingId id = next_binding_++;
        axis_bindings_.push_back({id, std::move(modifiers), axis, action, options});
        return id;
    }

    bool InputBindings::unbind(const BindingId id) {
        const auto removed_buttons = std::erase_if(button_bindings_, [id](const auto& binding) { return binding.id == id; });
        const auto removed_axes = std::erase_if(axis_bindings_, [id](const auto& binding) { return binding.id == id; });
        return removed_buttons + removed_axes != 0;
    }

    void InputBindings::unbind_action(const ActionId action) {
        std::erase_if(button_bindings_, [action](const auto& binding) { return binding.action == action; });
        std::erase_if(axis_bindings_, [action](const auto& binding) { return binding.action == action; });
    }

    void InputBindings::clear() {
        button_bindings_.clear();
        axis_bindings_.clear();
        held_buttons_.clear();
        physical_axes_.clear();
        pending_buttons_.clear();
        (void)publish_actions();
    }

    bool InputBindings::chord_active(const InputChord& chord) const {
        return std::ranges::all_of(chord.required, [this](const DeviceBind control) { return held_buttons_.contains(control); });
    }

    bool InputBindings::button_active(const ActionId action) const {
        return std::ranges::any_of(button_bindings_, [this, action](const auto& binding) {
            return binding.action == action && chord_active(binding.chord);
        });
    }

    std::unordered_map<ActionId, bool> InputBindings::evaluate_buttons() const {
        std::unordered_map<ActionId, bool> result;
        for (const auto& binding : button_bindings_) {
            auto& active = result[binding.action];
            active = active || chord_active(binding.chord);
        }
        return result;
    }

    std::unordered_map<ActionId, float> InputBindings::evaluate_axes() const {
        std::unordered_map<ActionId, float> result;
        for (const auto& binding : axis_bindings_) {
            float value = 0.0f;
            if (chord_active(binding.modifiers)) {
                if (const auto it = physical_axes_.find(binding.axis); it != physical_axes_.end())
                    value = it->second;
            }
            const float magnitude = std::abs(value);
            if (magnitude <= binding.options.dead_zone)
                value = 0.0f;
            else if (binding.options.dead_zone > 0.0f)
                value = std::copysign((magnitude - binding.options.dead_zone) / (1.0f - binding.options.dead_zone), value);
            result[binding.action] += value * binding.options.scale;
        }
        return result;
    }

    std::shared_ptr<const ActionSnapshot> InputBindings::publish_actions() {
        const auto prior = action_snapshot();
        auto next = std::make_shared<ActionSnapshot>();
        next->poll_ = next_poll_++;

        // Complete all bindings before publishing. Retain one release/zero sample for actions unbound
        // since the last poll, so consumers can observe their departure instead of a silent disappearance.
        auto buttons = evaluate_buttons();
        for (const auto& [id, state] : prior->buttons_) {
            if (state.current)
                buttons.try_emplace(id, false);
        }
        for (const auto& [id, current] : buttons) {
            const bool previous = prior->button(id).current;
            const auto pending = pending_buttons_.find(id);
            const bool pressed = pending != pending_buttons_.end() && pending->second.pressed;
            const bool released = pending != pending_buttons_.end() && pending->second.released;
            next->buttons_.emplace(id,
                                   ButtonActionState{current, previous, pressed || (current && !previous),
                                                     released || (!current && previous)});
        }

        auto axes = evaluate_axes();
        for (const auto& [id, state] : prior->axes_) {
            if (state.current != 0.0f)
                axes.try_emplace(id, 0.0f);
        }
        for (const auto& [id, current] : axes)
            next->axes_.emplace(id, AxisActionState{current, prior->axis(id).current});

        pending_buttons_.clear();
        std::shared_ptr<const ActionSnapshot> completed = std::move(next);
        published_.store(completed, std::memory_order_release);
        return completed;
    }

    std::shared_ptr<const ActionSnapshot> InputBindings::action_snapshot() const { return published_.load(std::memory_order_acquire); }

    void InputBindings::on_axis(const DeviceBind binding, const float value) {
        if (value == 0.0f)
            physical_axes_.erase(binding);
        else
            physical_axes_.insert_or_assign(binding, value);
    }

    void InputBindings::refresh_button(const ActionId action) {
        auto [it, inserted] = pending_buttons_.try_emplace(action);
        auto& pending = it->second;
        if (inserted)
            pending.active = action_snapshot()->button(action).current;

        const bool active = button_active(action);
        if (active == pending.active)
            return;
        pending.active = active;
        if (active)
            pending.pressed = true;
        else
            pending.released = true;
    }

    void InputBindings::on_button(const DeviceBind control, const bool held) {
        if (held_buttons_.contains(control) == held)
            return;
        if (held)
            held_buttons_.insert(control);
        else
            held_buttons_.erase(control);

        // Reevaluate only actions using this control. Preserve both edges when an action is
        // pressed and released before publication, even when the last sample was inactive.
        for (const auto& binding : button_bindings_) {
            if (std::ranges::find(binding.chord.required, control) != binding.chord.required.end())
                refresh_button(binding.action);
        }
    }
} // namespace CE::Input
