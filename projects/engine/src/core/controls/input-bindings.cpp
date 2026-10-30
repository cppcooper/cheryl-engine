#include <core/controls/input-bindings.h>
#include "action-context-internal.h"
#include <internals/exceptions.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
    float scaled_axis(const float value, const CE::Input::Actions::AxisOptions options) {
        const float magnitude = std::abs(value);
        float adjusted = value;
        if (magnitude <= options.dead_zone)
            adjusted = 0.0f;
        else if (options.dead_zone > 0.0f)
            adjusted = std::copysign((magnitude - options.dead_zone) / (1.0f - options.dead_zone), value);
        const float result = adjusted * options.scale;
        if (!std::isfinite(result))
            throw CE::Exceptions::invalid_args(CE_HERE, "Scaled input axes must remain finite");
        return result;
    }
}

namespace CE::Input::Actions {
    Bindings::~Bindings() {
        if (!contexts_)
            return;
        const std::lock_guard lock(contexts_->mutex);
        contexts_->closed = true;
        for (const auto* entries : {&contexts_->pending, &contexts_->active})
            for (const auto& [id, entry] : *entries) {
                static_cast<void>(id);
                if (const auto token = entry.token.lock())
                    token->requested.store(false, std::memory_order_release);
            }
        contexts_->pending.clear();
        contexts_->active.clear();
    }

    ContextSubmission Bindings::action_contexts() {
        if (!contexts_)
            contexts_ = std::make_shared<Detail::ContextsState>();
        return ContextSubmission(contexts_);
    }

    void Bindings::begin_poll() {
        if (poll_started_)
            return;
        if (contexts_) {
            const std::lock_guard lock(contexts_->mutex);
            // Latch departures before installing new namespaces. A reset racing
            // a successful new installation belongs to the following boundary.
            for (auto& [id, entry] : contexts_->active) {
                static_cast<void>(id);
                const auto token = entry.token.lock();
                if (!token || !token->requested.load(std::memory_order_acquire)) {
                    entry.removing = true;
                    // Policy setup can change a device gate before this boundary.
                    // Departures resolve from the last publication, without a
                    // transient resumed press from that policy prelude.
                    entry.bindings->pending_buttons_.clear();
                    entry.bindings->relative_actions_.clear();
                    entry.bindings->button_bindings_.clear();
                    entry.bindings->axis_bindings_.clear();
                }
            }
            for (auto it = contexts_->pending.begin(); it != contexts_->pending.end();) {
                const auto token = it->second.token.lock();
                if (!token || !token->requested.load(std::memory_order_acquire)) {
                    it = contexts_->pending.erase(it);
                    continue;
                }
                auto& mapper = *it->second.bindings;
                mapper.held_buttons_ = held_buttons_;
                mapper.physical_axes_ = physical_axes_;
                mapper.disabled_devices_ = disabled_devices_;
                // A new namespace starts from an inactive sample at this mapper's
                // previous poll, never from already observed relative activity.
                auto baseline = std::make_shared<Snapshot>();
                baseline->poll_ = action_snapshot()->poll();
                baseline->observed_at_ = action_snapshot()->observed_at();
                mapper.published_.store(std::move(baseline), std::memory_order_release);
                mapper.pending_buttons_.clear();
                mapper.relative_actions_.clear();
                for (const auto& [action, held] : mapper.evaluate_buttons())
                    if (held)
                        mapper.refresh_button(action);
                contexts_->active.emplace(it->first, it->second);
                it = contexts_->pending.erase(it);
            }
        }
        poll_started_ = true;
    }

    void Bindings::clear_contexts() {
        if (!contexts_)
            return;
        const std::lock_guard lock(contexts_->mutex);
        for (auto* entries : {&contexts_->pending, &contexts_->active})
            for (auto& [id, entry] : *entries) {
                static_cast<void>(id);
                if (const auto token = entry.token.lock())
                    token->requested.store(false, std::memory_order_release);
                entry.removing = true;
                auto& mapper = *entry.bindings;
                mapper.button_bindings_.clear();
                mapper.axis_bindings_.clear();
                mapper.held_buttons_.clear();
                mapper.physical_axes_.clear();
                mapper.disabled_devices_.clear();
                mapper.relative_actions_.clear();
                mapper.pending_buttons_.clear();
            }
        contexts_->pending.clear();
    }

    BindingId Bindings::bind_button(const CE::Input::Control control, const Id action) {
        return bind_button(Chord{{control}}, action);
    }

    BindingId Bindings::bind_button(Chord chord, const Id action) {
        if (relative_actions_.contains(action))
            throw Exceptions::invalid_args(CE_HERE, "Publish pending relative input before rebinding its action as a button");
        if (chord.required.empty())
            throw Exceptions::invalid_args(CE_HERE, "A button action needs at least one physical control");
        if (std::ranges::any_of(chord.required, [](const auto control) { return control.control == CE::Input::invalid_control; }))
            throw Exceptions::invalid_args(CE_HERE, "A button action requires known controls");
        if (std::ranges::any_of(axis_bindings_, [action](const auto& binding) { return binding.action == action; }))
            throw Exceptions::invalid_args(CE_HERE, "An action cannot be bound as both a button and an axis");
        const BindingId id = next_binding_++;
        button_bindings_.push_back({id, std::move(chord), action});
        return id;
    }

    BindingId Bindings::bind_axis(const CE::Input::Control axis, const Id action, const AxisOptions options) {
        return bind_axis(Chord{}, axis, action, options);
    }

    BindingId Bindings::bind_axis(Chord modifiers, const CE::Input::Control axis, const Id action, const AxisOptions options) {
        if (axis.control == CE::Input::invalid_control ||
            std::ranges::any_of(modifiers.required, [](const auto control) { return control.control == CE::Input::invalid_control; }))
            throw Exceptions::invalid_args(CE_HERE, "An axis action requires known controls");
        if (relative_actions_.contains(action) && options.kind != CE::Input::AxisKind::Relative)
            throw Exceptions::invalid_args(CE_HERE, "Publish pending relative input before changing its axis kind");
        if (!std::isfinite(options.scale) || !std::isfinite(options.dead_zone) || options.dead_zone < 0.0f || options.dead_zone >= 1.0f ||
            (options.kind != CE::Input::AxisKind::Absolute && options.kind != CE::Input::AxisKind::Relative))
            throw Exceptions::invalid_args(CE_HERE, "An axis requires a finite scale and a dead zone in [0, 1)");
        if (std::ranges::any_of(button_bindings_, [action](const auto& binding) { return binding.action == action; }))
            throw Exceptions::invalid_args(CE_HERE, "An action cannot be bound as both a button and an axis");
        if (std::ranges::any_of(axis_bindings_, [action, options](const auto& binding) {
                return binding.action == action && binding.options.kind != options.kind;
            }))
            throw Exceptions::invalid_args(CE_HERE, "An axis action cannot combine absolute and relative bindings");
        const BindingId id = next_binding_++;
        axis_bindings_.push_back({id, std::move(modifiers), axis, action, options});
        return id;
    }

    bool Bindings::unbind(const BindingId id) {
        const auto removed_buttons = std::erase_if(button_bindings_, [id](const auto& binding) { return binding.id == id; });
        const auto removed_axes = std::erase_if(axis_bindings_, [id](const auto& binding) { return binding.id == id; });
        return removed_buttons + removed_axes != 0;
    }

    void Bindings::unbind_action(const Id action) {
        std::erase_if(button_bindings_, [action](const auto& binding) { return binding.action == action; });
        std::erase_if(axis_bindings_, [action](const auto& binding) { return binding.action == action; });
    }

    void Bindings::clear() {
        clear_contexts();
        button_bindings_.clear();
        axis_bindings_.clear();
        held_buttons_.clear();
        disabled_devices_.clear();
        physical_axes_.clear();
        relative_actions_.clear();
        pending_buttons_.clear();
        (void)publish_actions();
    }

    void Bindings::set_device_enabled(const CE::Input::DeviceId device, const bool enabled) {
        if (contexts_)
            for (auto& [id, entry] : contexts_->active) {
                static_cast<void>(id);
                const auto token = entry.token.lock();
                if (!entry.removing && (poll_started_ || (token && token->requested.load(std::memory_order_acquire))))
                    entry.bindings->set_device_enabled(device, enabled);
            }
        const bool changed = enabled ? disabled_devices_.erase(device) != 0 : disabled_devices_.insert(device).second;
        if (!changed)
            return;
        for (const auto& binding : button_bindings_)
            if (std::ranges::any_of(binding.chord.required, [device](const CE::Input::Control control) {
                    return control.device == device;
                }))
                refresh_button(binding.action);
    }

    bool Bindings::chord_active(const Chord& chord) const {
        return std::ranges::all_of(chord.required, [this](const CE::Input::Control control) {
            return !disabled_devices_.contains(control.device) && held_buttons_.contains(control);
        });
    }

    bool Bindings::button_active(const Id action) const {
        return std::ranges::any_of(button_bindings_, [this, action](const auto& binding) {
            return binding.action == action && chord_active(binding.chord);
        });
    }

    std::unordered_map<Id, bool> Bindings::evaluate_buttons() const {
        std::unordered_map<Id, bool> result;
        for (const auto& binding : button_bindings_) {
            auto& active = result[binding.action];
            active = active || chord_active(binding.chord);
        }
        return result;
    }

    std::unordered_map<Id, float> Bindings::evaluate_axes() const {
        // Relative activity is mapped when delivered, including motion before an
        // unbind. Absolute values are evaluated against the final physical state.
        std::unordered_map<Id, float> result = relative_actions_;
        for (const auto& binding : axis_bindings_) {
            if (binding.options.kind == CE::Input::AxisKind::Relative) {
                result.try_emplace(binding.action, 0.0f);
                continue;
            }
            float value = 0.0f;
            if (!disabled_devices_.contains(binding.axis.device) && chord_active(binding.modifiers)) {
                if (const auto it = physical_axes_.find(binding.axis); it != physical_axes_.end())
                    value = it->second;
            }
            const float combined = result[binding.action] + scaled_axis(value, binding.options);
            if (!std::isfinite(combined))
                throw Exceptions::invalid_args(CE_HERE, "Combined input axes must remain finite");
            result[binding.action] = combined;
        }
        return result;
    }

    std::shared_ptr<Snapshot> Bindings::make_snapshot(const std::chrono::steady_clock::time_point observed_at) {
        const auto prior = action_snapshot();
        if (observed_at < prior->observed_at())
            throw Exceptions::invalid_args(CE_HERE, "Input poll observation times must be monotonic");
        auto next = std::make_shared<Snapshot>();
        next->poll_ = next_poll_++;
        next->observed_at_ = observed_at;

        // Complete all bindings before publishing. Retain one release/zero sample for actions unbound
        // since the last poll, so consumers can observe their departure instead of a silent disappearance.
        auto buttons = evaluate_buttons();
        for (const auto& [id, state] : prior->buttons_) {
            if (state.current)
                buttons.try_emplace(id, false);
        }
        for (const auto& [id, pending] : pending_buttons_)
            buttons.try_emplace(id, false);
        for (const auto& [id, current] : buttons) {
            const auto previous = prior->button(id);
            ButtonPollState state;
            state.current = previous.current;
            state.previous = previous.current;
            state.hold_started_at = previous.hold_started_at;
            const auto transition = [&](const bool held) {
                if (held == state.current)
                    return;
                state.current = held;
                if (held) {
                    ++state.press_count;
                    state.hold_started_at = observed_at;
                } else {
                    ++state.release_count;
                    state.completed_holds.emplace_back(observed_at - state.hold_started_at.value_or(observed_at));
                    state.hold_started_at.reset();
                }
            };
            if (const auto pending = pending_buttons_.find(id); pending != pending_buttons_.end())
                for (const bool held : pending->second.transitions)
                    transition(held);
            // Binding installation/removal also changes semantic state without
            // requiring another physical notification from the device.
            transition(current);
            state.pressed_this_poll = state.press_count != 0;
            state.released_this_poll = state.release_count != 0;
            next->buttons_.emplace(id, std::move(state));
        }

        auto axes = evaluate_axes();
        std::unordered_map<Id, CE::Input::AxisKind> kinds;
        for (const auto& binding : axis_bindings_)
            kinds.emplace(binding.action, binding.options.kind);
        for (const auto& [action, value] : relative_actions_)
            kinds.try_emplace(action, CE::Input::AxisKind::Relative);
        for (const auto& [id, state] : prior->axes_) {
            if (state.kind == CE::Input::AxisKind::Absolute && state.current != 0.0f) {
                axes.try_emplace(id, 0.0f);
                kinds.try_emplace(id, state.kind);
            }
        }
        for (const auto& [id, current] : axes) {
            const auto kind = kinds.at(id);
            const auto previous = prior->axis(id);
            next->axes_.emplace(
                id, AxisPollState{current, kind == CE::Input::AxisKind::Absolute && previous.kind == kind ? previous.current : 0.0f, kind}
            );
        }

        return next;
    }

    void Bindings::commit_snapshot(std::shared_ptr<const Snapshot> snapshot) noexcept {
        pending_buttons_.clear();
        relative_actions_.clear();
        published_.store(std::move(snapshot), std::memory_order_release);
        poll_started_ = false;
    }

    std::shared_ptr<const Snapshot> Bindings::publish_actions(const std::chrono::steady_clock::time_point observed_at) {
        begin_poll();
        auto next = make_snapshot(observed_at);
        if (contexts_) {
            // Build every namespace before committing any pending transitions.
            // A failed candidate leaves the complete previous publication intact.
            for (const auto& [id, entry] : contexts_->active) {
                auto sample = entry.bindings->make_snapshot(observed_at);
                sample->poll_ = next->poll_;
                next->contexts_.emplace(id, std::move(sample));
            }
            for (const auto& [id, entry] : contexts_->active)
                entry.bindings->commit_snapshot(next->contexts_.at(id));
            std::erase_if(contexts_->active, [](const auto& item) { return item.second.removing; });
        }
        commit_snapshot(next);
        return next;
    }

    std::shared_ptr<const Snapshot> Bindings::action_snapshot() const {
        return published_.load(std::memory_order_acquire);
    }

    void Bindings::on_axis(const CE::Input::Control binding, const float value) {
        begin_poll();
        if (!std::isfinite(value))
            throw Exceptions::invalid_args(CE_HERE, "Input axes must have finite values");
        if (contexts_)
            for (auto& [id, entry] : contexts_->active) {
                static_cast<void>(id);
                if (!entry.removing)
                    entry.bindings->on_axis(binding, value);
            }
        if (value == 0.0f)
            physical_axes_.erase(binding);
        else
            physical_axes_.insert_or_assign(binding, value);
    }

    void Bindings::on_delta(const CE::Input::Control binding, const float delta) {
        begin_poll();
        if (!std::isfinite(delta))
            throw Exceptions::invalid_args(CE_HERE, "Input deltas must have finite values");
        if (contexts_)
            for (auto& [id, entry] : contexts_->active) {
                static_cast<void>(id);
                if (!entry.removing)
                    entry.bindings->on_delta(binding, delta);
            }
        if (disabled_devices_.contains(binding.device))
            return;
        // Apply modifiers at the observation, not at the end of a later poll:
        // releasing Ctrl after Ctrl+wheel must not erase the earlier movement.
        for (const auto& mapping : axis_bindings_) {
            if (mapping.axis != binding || mapping.options.kind != CE::Input::AxisKind::Relative || !chord_active(mapping.modifiers))
                continue;
            auto& accumulated = relative_actions_[mapping.action];
            const float combined = accumulated + scaled_axis(delta, mapping.options);
            if (!std::isfinite(combined))
                throw Exceptions::invalid_args(CE_HERE, "Accumulated input deltas must remain finite");
            accumulated = combined;
        }
    }

    void Bindings::refresh_button(const Id action) {
        auto [it, inserted] = pending_buttons_.try_emplace(action);
        auto& pending = it->second;
        if (inserted)
            pending.active = action_snapshot()->button(action).current;

        const bool active = button_active(action);
        if (active == pending.active)
            return;
        pending.active = active;
        pending.transitions.push_back(active);
    }

    void Bindings::on_button(const CE::Input::Control control, const bool held) {
        begin_poll();
        if (contexts_)
            for (auto& [id, entry] : contexts_->active) {
                static_cast<void>(id);
                if (!entry.removing)
                    entry.bindings->on_button(control, held);
            }
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
} // namespace CE::Input::Actions
