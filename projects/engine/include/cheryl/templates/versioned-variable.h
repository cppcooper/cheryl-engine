#pragma once

#include <concepts>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <utility>

namespace CE::State {

    /** Owned value/revision snapshot sampled under one storage lock.
     * @tparam T Copied value type; ownership of its pointees follows T itself.
     */
    template <typename T> struct VersionedSnapshot {
        T value; ///< Value copied at the same instant as revision; not a live view.
        std::uint64_t revision; ///< Change generation, starting at zero; unsigned wrap is not prevented.
    };

    /** Synchronizes one value and its revision as a unit. Repeated equal sets do not advance the revision.
 * Readers can retain a revision and wait for a later change without missing a change between reads.
 * Comparison/copy/assignment execute under the storage mutex and must not reenter
 * this variable. Their exceptions propagate; throwing assignment follows T's own
 * failure guarantee and may change a value without advancing revision/notifying.
 * Keep the variable alive through all readers/waiters; destruction is not cancellation.
 * The mutex makes this storage noncopyable/nonmovable; snapshots copy values instead.
 * @tparam T Copyable, assignable, equality-comparable stored value type.
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

        [[nodiscard]] ::CE::State::VersionedSnapshot<T> snapshot() const {
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

        [[nodiscard]] ::CE::State::VersionedSnapshot<T> wait_for_change(std::uint64_t since) const {
            std::unique_lock lock(mutex_);
            changed_.wait(lock, [&] { return revision_ != since; });
            return {value_, revision_};
        }
    };

}

/** @fn template<typename T> CE::State::VersionedVariable<T>::VersionedVariable(T initial)
 * @brief Publishes the initial owned value with revision zero.
 * @param initial Value moved into storage; construction/copy failures propagate.
 * No notification is emitted during construction; the object must outlive every waiter.
 */

/** @fn template<typename T> void CE::State::VersionedVariable<T>::set(T next)
 * @brief Publishes a changed value and wakes waiters after unlocking.
 * @param next Candidate value, copied/moved into owned storage.
 * An equal value leaves revision unchanged. Comparison/assignment happen under the
 * mutex and must not reenter this variable. Their exceptions propagate; a throwing
 * assignment follows T's guarantee and may modify value without advancing revision.
 * @see [State utilities](cheryl-guide:runtime/utility-contracts)
 */

/** @fn template<typename T> CE::State::VersionedSnapshot<T> CE::State::VersionedVariable<T>::snapshot() const
 * @brief Copies the value and revision under one lock.
 * @return Independent snapshot; copying T can throw. Its pointees still follow T's ownership.
 * The lock is released before return and does not protect use of the returned value.
 */

/** @fn template<typename T> T CE::State::VersionedVariable<T>::get() const
 * @brief Copies the current value under its storage lock.
 * @return Owned T copy; no revision or retained lock accompanies it. Copy failures propagate.
 */

/** @fn template<typename T> std::uint64_t CE::State::VersionedVariable<T>::revision() const
 * @brief Samples the current change revision under its storage lock.
 * @return Unsigned revision, incremented only for a successfully assigned different value.
 * Separate get()/revision() calls are not one atomic snapshot; use snapshot() for that.
 */

/** @fn template<typename T> CE::State::VersionedSnapshot<T> CE::State::VersionedVariable<T>::wait_for_change(std::uint64_t since) const
 * @brief Waits until the current revision differs, then copies a consistent snapshot.
 * @param since Previously sampled revision; returns immediately when it already differs.
 * @return Current owned value/revision, not every intermediate update.
 * Notifications coalesce. There is no timeout or shutdown cancellation: keep storage
 * alive and arrange a changed set() when waiters must finish. Copy failures propagate.
 */
