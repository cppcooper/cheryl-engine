#pragma once

#include "versioned-variable.h"

#include <array>
#include <cstdint>
#include <functional>
#include <utility>

/** Adds immediate observer callbacks to synchronized, versioned storage. Every set() invokes
 * observers on the calling thread, even when the value is unchanged. Waiters wake only for a
 * changed value. Use VersionedVariable directly when callback thread affinity is undesirable.
 */
template <typename T, std::uint8_t Observers = 1>
struct ObservedVariable {
    using Callback = std::function<void(const T&)>;

    explicit ObservedVariable(T value, std::array<Callback, Observers> observers)
        : value_(std::move(value)), callbacks(std::move(observers)) {}

    ObservedVariable& operator=(T value) {
        set(std::move(value));
        return *this;
    }

    void set(T value) {
        value_.set(value);
        // The argument belongs to this set even if another writer publishes before these callbacks run.
        for (auto& callback : callbacks) {
            if (callback)
                callback(value);
        }
    }

    [[nodiscard]] T get() const { return value_.get(); }
    [[nodiscard]] VersionedSnapshot<T> snapshot() const { return value_.snapshot(); }
    [[nodiscard]] std::uint64_t revision() const { return value_.revision(); }
    [[nodiscard]] VersionedSnapshot<T> wait_for_change(std::uint64_t since) const { return value_.wait_for_change(since); }
    void wait_until_change() const { (void)wait_for_change(revision()); }

protected:
    VersionedVariable<T> value_;
    std::array<Callback, Observers> callbacks;
};
