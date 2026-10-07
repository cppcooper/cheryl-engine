#pragma once

#include "versioned-variable.h"

#include <array>
#include <cstdint>
#include <functional>
#include <utility>

/** Adds immediate observer callbacks to synchronized, versioned storage. Every set() invokes
 * observers on the calling thread, even when the value is unchanged. Waiters wake only for a
 * changed value. Use VersionedVariable directly when callback thread affinity is undesirable.
 * Callbacks run in array order after releasing the storage mutex, with this set's
 * argument, not a later snapshot. Their const reference is borrowed for the call.
 * Concurrent/reentrant sets can overlap callbacks; callbacks protect their own targets.
 * A throw propagates after publication and skips later observers without rollback.
 */
template <typename T, std::uint8_t Observers = 1> class ObservedVariable {
public:
    using Callback = std::function<void(const T&)>;

private:
    VersionedVariable<T> value_;
    std::array<Callback, Observers> callbacks_;

public:
    explicit ObservedVariable(T value, std::array<Callback, Observers> observers)
    : value_(std::move(value)), callbacks_(std::move(observers)) {}

    ObservedVariable& operator=(T value) {
        set(std::move(value));
        return *this;
    }

    void set(T value) {
        value_.set(value);
        // The argument belongs to this set even if another writer publishes before these callbacks run.
        for (const auto& callback : callbacks_) {
            if (callback)
                callback(value);
        }
    }

    [[nodiscard]] T get() const { return value_.get(); }
    [[nodiscard]] VersionedSnapshot<T> snapshot() const { return value_.snapshot(); }
    [[nodiscard]] std::uint64_t revision() const { return value_.revision(); }
    [[nodiscard]] VersionedSnapshot<T> wait_for_change(std::uint64_t since) const { return value_.wait_for_change(since); }
    // Begins at this call's sampled revision; does not observe an earlier unread change.
    void wait_until_change() const { (void)wait_for_change(revision()); }
};
