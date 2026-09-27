#include <core/controls/input-bindings.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace CE::Input {
    void InputBindings::bind_axis(const DeviceBind binding, std::function<void(float, float)> callback) {
        if (callback)
            axis_callbacks_.insert_or_assign(binding, std::move(callback));
        else
            axis_callbacks_.erase(binding);
    }

    void InputBindings::bind_button(const DeviceBind binding, std::function<void(bool, bool)> callback) {
        if (callback)
            button_callbacks_.insert_or_assign(binding, std::move(callback));
        else
            button_callbacks_.erase(binding);
    }

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
        axis_callbacks_.clear();
        button_callbacks_.clear();
        button_bindings_.clear();
        axis_bindings_.clear();
        physical_buttons_.clear();
        physical_axes_.clear();
        last_button_active_.clear();
        pending_presses_.clear();
        pending_releases_.clear();
        (void)publish_actions();
    }

    bool InputBindings::chord_active(const InputChord& chord) const {
        return std::ranges::all_of(chord.required, [this](const DeviceBind control) {
            const auto found = physical_buttons_.find(control);
            return found != physical_buttons_.end() && found->second;
        });
    }

    std::unordered_map<ActionId, bool> InputBindings::evaluate_buttons() const {
        std::unordered_map<ActionId, bool> result;
        for (const auto& binding : button_bindings_)
            result[binding.action] = result[binding.action] || chord_active(binding.chord);
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
        last_button_active_ = buttons;
        for (const auto& [id, state] : prior->buttons_) {
            if (state.current)
                buttons.try_emplace(id, false);
        }
        for (const auto& [id, current] : buttons) {
            const bool previous = prior->button(id).current;
            next->buttons_.emplace(id,
                                   ButtonActionState{current, previous, pending_presses_.contains(id) || (current && !previous),
                                                     pending_releases_.contains(id) || (!current && previous)});
        }

        auto axes = evaluate_axes();
        for (const auto& [id, state] : prior->axes_) {
            if (state.current != 0.0f)
                axes.try_emplace(id, 0.0f);
        }
        for (const auto& [id, current] : axes)
            next->axes_.emplace(id, AxisActionState{current, prior->axis(id).current});

        pending_presses_.clear();
        pending_releases_.clear();
        std::shared_ptr<const ActionSnapshot> completed = std::move(next);
        published_.store(completed, std::memory_order_release);
        return completed;
    }

    std::shared_ptr<const ActionSnapshot> InputBindings::action_snapshot() const { return published_.load(std::memory_order_acquire); }

    void InputBindings::on_axis(const DeviceBind binding, const float old_value, const float new_value) {
        physical_axes_[binding] = new_value;
        if (const auto it = axis_callbacks_.find(binding); it != axis_callbacks_.end()) {
            // Copy the callable before invocation: a listener may change its
            // own mapping during delivery and invalidate the map iterator.
            const auto callback = it->second;
            callback(old_value, new_value);
        }
    }

    void InputBindings::on_button(const DeviceBind binding, const bool old_value, const bool new_value) {
        physical_buttons_[binding] = new_value;
        // Compare the semantic result after each physical transition. A quick press and release in
        // one poll still produces both edges, even if its final held state matches the prior poll.
        auto active = evaluate_buttons();
        for (const auto& [id, before] : last_button_active_) {
            if (before && !active[id])
                pending_releases_[id] = true;
        }
        for (const auto& [id, now] : active) {
            if (now && !last_button_active_[id])
                pending_presses_[id] = true;
        }
        last_button_active_ = std::move(active);
        if (const auto it = button_callbacks_.find(binding); it != button_callbacks_.end()) {
            // Keep this invocation alive even if the callback replaces or
            // removes its binding from the map.
            const auto callback = it->second;
            callback(old_value, new_value);
        }
    }
} // namespace CE::Input
