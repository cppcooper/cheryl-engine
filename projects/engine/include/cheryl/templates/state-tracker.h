#pragma once

#include <concepts>
#include <utility>

/** Caller-owned successive samples without synchronization. Accessors borrow this
 * tracker's storage and must not race update(). update() assigns previous before
 * current; throwing T assignment can leave partial state rather than rolling back.
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
