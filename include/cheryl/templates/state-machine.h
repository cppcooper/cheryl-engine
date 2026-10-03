#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

/** A transition table for one typed state domain. Rules are tested in insertion order; the
 * first matching rule whose guard succeeds wins. Guards and actions may inspect other machines,
 * allowing a game to compose state domains without requiring a controller or machine hierarchy.
 * The machine is owned by its caller and is not synchronized or reentrant during trigger().
 */
template <typename State, typename Trigger>
    requires std::copy_constructible<State> && std::copy_constructible<Trigger> && std::equality_comparable<State> &&
             std::equality_comparable<Trigger>
class StateMachine {
public:
    using Guard = std::function<bool()>;
    using Action = std::function<void()>;

    explicit StateMachine(State initial)
    : state_(std::move(initial)) {}

    void add_transition(State from, Trigger trigger, State to, Guard guard = {}, Action action = {}) {
        transitions_.push_back({std::move(from), std::move(trigger), std::move(to), std::move(guard), std::move(action)});
    }

    void on_enter(State state, Action action) { set_hook(enter_, std::move(state), std::move(action)); }
    void on_exit(State state, Action action) { set_hook(exit_, std::move(state), std::move(action)); }

    [[nodiscard]] const State& state() const { return state_; }

    bool trigger(const Trigger& event) {
        for (std::size_t index = 0; index < transitions_.size(); ++index) {
            // Guards can also register rules, so retain this candidate independently of storage.
            const Rule rule = transitions_[index];
            if (rule.from != state_ || rule.trigger != event || (rule.guard && !rule.guard()))
                continue;

            // Hooks may replace hooks or add transitions; take callable copies before invoking them.
            const State from = state_;
            const State to = rule.to;
            const Action action = rule.action;
            const Action exiting = hook_for(exit_, from);
            const Action entering = hook_for(enter_, to);
            if (exiting)
                exiting();
            state_ = to;
            if (action)
                action();
            if (entering)
                entering();
            return true;
        }
        return false;
    }

private:
    struct Rule {
        State from;
        Trigger trigger;
        State to;
        Guard guard;
        Action action;
    };
    using Hook = std::pair<State, Action>;

    static void set_hook(std::vector<Hook>& hooks, State state, Action action) {
        for (auto& hook : hooks) {
            if (hook.first == state) {
                hook.second = std::move(action);
                return;
            }
        }
        hooks.emplace_back(std::move(state), std::move(action));
    }

    static Action hook_for(const std::vector<Hook>& hooks, const State& state) {
        for (const auto& [key, action] : hooks) {
            if (key == state)
                return action;
        }
        return {};
    }

    State state_;
    std::vector<Rule> transitions_;
    std::vector<Hook> enter_;
    std::vector<Hook> exit_;
};
