#pragma once

#include <concepts>
#include <utility>

namespace CE::State {

    /** Caller-owned successive samples without synchronization. Accessors borrow this
 * tracker's storage and must not race update(). update() assigns previous before
 * current; throwing T assignment can leave partial state rather than rolling back.
 * @tparam T Copy-constructible, assignable sample type; optional queries add their own constraints.
 */
    template <typename T>
        requires std::copy_constructible<T> && std::assignable_from<T&, T>
    class StateTracker {
    public:
        explicit StateTracker(T initial)
        : previous_(initial), current_(std::move(initial)) {}

        void update(T next) {
            previous_ = current_;
            current_ = std::move(next);
        }

        [[nodiscard]] const T& current() const { return current_; }
        [[nodiscard]] const T& previous() const { return previous_; }
        [[nodiscard]] bool changed() const
            requires std::equality_comparable<T>
        {
            return current_ != previous_;
        }

        [[nodiscard]] auto delta() const
            requires requires(const T& a, const T& b) { a - b; }
        {
            return current_ - previous_;
        }

    private:
        T previous_;
        T current_;
    };

}

/** @fn template<typename T> CE::State::StateTracker<T>::StateTracker(T initial)
 * @brief Initializes previous and current to the same sample.
 * @param initial Value copied to previous and moved to current; type failures propagate.
 */

/** @fn template<typename T> void CE::State::StateTracker<T>::update(T next)
 * @brief Shifts current into previous, then installs the next sample.
 * @param next New current value, moved into owned storage.
 * The object is unsynchronized. Assignment failures propagate and can leave a partial
 * shift; no rollback is performed. Existing references still name this storage.
 */

/** @fn template<typename T> const T& CE::State::StateTracker<T>::current() const
 * @brief Borrows the current sample.
 * @return Current sample; initially equal to previous.
 * The reference lives with this tracker and observes later assignments; copy it to
 * retain an independent value. It must not race update().
 */

/** @fn template<typename T> const T& CE::State::StateTracker<T>::previous() const
 * @brief Borrows the previous sample.
 * @return Sample preceding the last successful update; initially equal to current.
 * The reference lives with this tracker and observes later assignments; copy it to
 * retain an independent value. It must not race update().
 */

/** @fn template<typename T> bool CE::State::StateTracker<T>::changed() const
 * @brief Compares the current and previous samples.
 * @return True when current != previous, not a latched unread-change flag.
 * Requires equality-comparable T. Comparison failures propagate; no sample is changed.
 */

/** @fn template<typename T> auto CE::State::StateTracker<T>::delta() const
 * @brief Computes current minus previous using T's subtraction.
 * @return The expression result by value; its units/ownership follow that result type.
 * Available only when subtraction is well-formed. Overflow and exceptions follow T;
 * this adds no saturation or validation.
 * @see [State utilities](cheryl-guide:runtime/utility-contracts)
 */
