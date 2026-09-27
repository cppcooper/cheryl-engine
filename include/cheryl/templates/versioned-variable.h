#pragma once

#include <concepts>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <utility>

template <typename T>
struct VersionedSnapshot {
    T value;
    std::uint64_t revision;
};

/** Synchronizes one value and its revision as a unit. Repeated equal sets do not advance the revision.
 * Readers can retain a revision and wait for a later change without missing a change between reads.
 */
template <typename T>
    requires std::copy_constructible<T> && std::assignable_from<T&, T> && std::equality_comparable<T>
class VersionedVariable {
public:
    explicit VersionedVariable(T initial) : value_(std::move(initial)) {}

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

    [[nodiscard]] T get() const { return snapshot().value; }
    [[nodiscard]] std::uint64_t revision() const { return snapshot().revision; }

    [[nodiscard]] VersionedSnapshot<T> wait_for_change(std::uint64_t since) const {
        std::unique_lock lock(mutex_);
        changed_.wait(lock, [&] { return revision_ != since; });
        return {value_, revision_};
    }

private:
    mutable std::mutex mutex_;
    mutable std::condition_variable changed_;
    T value_;
    std::uint64_t revision_ = 0;
};
