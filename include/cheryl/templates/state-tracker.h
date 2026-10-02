#pragma once

#include <concepts>
#include <utility>

/** Tracks two successive values of one logical state. update() advances the sample boundary. */
template <typename T>
    requires std::copy_constructible<T> && std::assignable_from<T&, T>
class StateTracker {
public:
    explicit StateTracker(T initial) : previous_(initial), current_(std::move(initial)) {}

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
