#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace CE::State {

    /** A transition table for one typed state domain. Rules are tested in insertion order; the
 * first matching rule whose guard succeeds wins. Guards and actions may inspect other machines,
 * allowing a game to compose state domains without requiring a controller or machine hierarchy.
 * The machine is owned by its caller and is not synchronized or reentrant during trigger().
 * Stored callbacks own their captures; borrowed targets must survive use/removal.
 * state() borrows this machine. Guards may add rules for later consideration in
 * the same trigger; hooks for a chosen transition are copied before calling exit.
 * Order is exit -> state assignment -> action -> enter. Exceptions propagate and
 * stop that sequence: exit failure keeps old state, action/enter failure keeps new
 * state; throwing State assignment follows that type's guarantee. No rollback.
 * @tparam State Copyable equality-comparable state identity; transitions assign it.
 * @tparam Trigger Copyable equality-comparable event identity, distinct from State's domain.
 * @see [State utilities](cheryl-guide:runtime/utility-contracts)
 */
    template <typename State, typename Trigger>
        requires std::copy_constructible<State> && std::copy_constructible<Trigger> && std::equality_comparable<State> &&
                 std::equality_comparable<Trigger>
    class StateMachine {
    public:
        /** Owned synchronous predicate; an empty guard always permits its matching rule. */
        using Guard = std::function<bool()>;
        /** Owned synchronous transition/entry/exit callback; empty means no callback. */
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

}

/** @fn template<typename State, typename Trigger> CE::State::StateMachine<State, Trigger>::StateMachine(State initial)
 * @brief Creates an empty rule/hook table at the supplied state.
 * @param initial Initial owned state, moved into the machine. No entry hook runs.
 * State construction failures propagate. The caller controls the machine's lifetime/thread.
 */

/** @fn template<typename State, typename Trigger> void CE::State::StateMachine<State, Trigger>::add_transition(State from, Trigger trigger, State to, Guard guard, Action action)
 * @brief Appends one rule; earlier matching rules retain priority.
 * @param from State from which this rule is eligible.
 * @param trigger Trigger value compared for equality at dispatch.
 * @param to State to install if this rule is selected.
 * @param guard Optional owned predicate, invoked only for matching from/trigger; empty means true.
 * @param action Optional owned callback after state assignment and before the entry hook.
 * Copies/moves and allocation can throw. Captured borrowed targets outlive invocation.
 * A guard may append rules considered later in the same trigger; do not reenter trigger().
 */

/** @fn template<typename State, typename Trigger> void CE::State::StateMachine<State, Trigger>::on_enter(State state, Action action)
 * @brief Registers or replaces the entry hook for a state.
 * @param state State identity to match, copied/moved into the hook table.
 * @param action Owned synchronous hook; empty disables that state's hook.
 * Registration does not invoke the hook. Replacement and allocation follow the
 * value/callable guarantees; borrowed captures must survive future use/removal.
 */

/** @fn template<typename State, typename Trigger> void CE::State::StateMachine<State, Trigger>::on_exit(State state, Action action)
 * @brief Registers or replaces the exit hook for a state.
 * @param state State identity to match, copied/moved into the hook table.
 * @param action Owned synchronous hook; empty disables that state's hook.
 * Registration does not invoke the hook. Replacement and allocation follow the
 * value/callable guarantees; borrowed captures must survive future use/removal.
 */

/** @fn template<typename State, typename Trigger> const State& CE::State::StateMachine<State, Trigger>::state() const
 * @brief Borrows the current state stored by this machine.
 * @return Reference valid while the machine lives; later transitions assign this same storage.
 * Copy the value to retain an independent sample. Access must not race mutation.
 */

/** @fn template<typename State, typename Trigger> bool CE::State::StateMachine<State, Trigger>::trigger(const Trigger& event)
 * @brief Executes the first matching rule whose guard permits it.
 * @param event Borrowed trigger identity, inspected synchronously.
 * @return True when a rule was executed, including self-transitions; false for no eligible rule.
 * The sequence is exit hook, state assignment, action, then entry hook. Hooks for the
 * chosen transition are copied before exit runs. Exceptions propagate without rollback:
 * exit failure retains the old state; action/entry failure retains the new state.
 * A throwing State assignment has its own partial-effect guarantee. No synchronization
 * or recursive trigger support is provided; hook changes affect later selections.
 */
