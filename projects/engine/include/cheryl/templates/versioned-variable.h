#pragma once

#include <concepts>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <utility>

// Owned value/revision copy; pointee lifetime still follows T's own ownership.
template <typename T> struct VersionedSnapshot {
    T value;
    std::uint64_t revision;
};

/** Synchronizes one value and its revision as a unit. Repeated equal sets do not advance the revision.
 * Readers can retain a revision and wait for a later change without missing a change between reads.
 * Comparison/copy/assignment execute under the storage mutex and must not reenter
 * this variable. Their exceptions propagate; throwing assignment follows T's own
 * failure guarantee and may change a value without advancing revision/notifying.
 * Keep the variable alive through all readers/waiters; destruction is not cancellation.
 */
template <typename T>
    requires std::copy_constructible<T> && std::assignable_from<T&, T> && std::equality_comparable<T>
class VersionedVariable {
    mutable std::mutex mutex_;
    mutable std::condition_variable changed_;
    T value_;
    std::uint64_t revision_ = 0;

public:
    explicit VersionedVariable(T initial)
    : value_(std::move(initial)) {}

    void set(T next) {
        {
            std::unique_lock lock(mutex_);
            if (value_ == next)
                return;
            value_ = std::move(next);
            ++revision_;
        }
        changed_.notify_all();
    }

    [[nodiscard]] VersionedSnapshot<T> snapshot() const {
        std::unique_lock lock(mutex_);
        return {value_, revision_};
    }

    [[nodiscard]] T get() const {
        std::unique_lock lock(mutex_);
        return value_;
    }

    [[nodiscard]] std::uint64_t revision() const {
        std::unique_lock lock(mutex_);
        return revision_;
    }

    // Wait indefinitely until revision differs (not necessarily exactly one update).
    // Notifications coalesce; a snapshot is current state, not a history of values.
    [[nodiscard]] VersionedSnapshot<T> wait_for_change(std::uint64_t since) const {
        std::unique_lock lock(mutex_);
        changed_.wait(lock, [&] { return revision_ != since; });
        return {value_, revision_};
    }
};
