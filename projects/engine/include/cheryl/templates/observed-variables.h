#pragma once

#include "versioned-variable.h"

#include <array>
#include <cstdint>
#include <functional>
#include <utility>

namespace CE::State {

    /** Adds immediate observer callbacks to synchronized, versioned storage. Every set() invokes
 * observers on the calling thread, even when the value is unchanged. Waiters wake only for a
 * changed value. Use CE::State::VersionedVariable directly when callback thread affinity is undesirable.
 * Callbacks run in array order after releasing the storage mutex, with this set's
 * argument, not a later snapshot. Their const reference is borrowed for the call.
 * Concurrent/reentrant sets can overlap callbacks; callbacks protect their own targets.
 * A throw propagates after publication and skips later observers without rollback.
 * @tparam T Value type satisfying VersionedVariable's copy/assignment/equality requirements.
 * @tparam Observers Fixed callback count, including zero; this object is noncopyable/nonmovable.
 */
    template <typename T, std::uint8_t Observers = 1> class ObservedVariable {
    public:
        /** Immediate observer receiving a borrowed reference to this set's argument. */
        using Callback = std::function<void(const T&)>;

    private:
        ::CE::State::VersionedVariable<T> value_;
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
        [[nodiscard]] ::CE::State::VersionedSnapshot<T> snapshot() const { return value_.snapshot(); }
        [[nodiscard]] std::uint64_t revision() const { return value_.revision(); }
        [[nodiscard]] ::CE::State::VersionedSnapshot<T> wait_for_change(std::uint64_t since) const { return value_.wait_for_change(since); }
        void wait_until_change() const { (void)wait_for_change(revision()); }
    };

}

/** @fn template<typename T, std::uint8_t Observers> CE::State::ObservedVariable<T, Observers>::ObservedVariable(T value, std::array<Callback, Observers> observers)
 * @brief Stores an initial value and fixed ordered observer list.
 * @param value Initial owned value, moved into synchronized storage at revision zero.
 * @param observers Owned callable array; empty entries are skipped and borrowed captures must remain alive.
 * Construction invokes no observer. Observers may be zero; no dynamic registration is provided.
 */

/** @fn template<typename T, std::uint8_t Observers> CE::State::ObservedVariable<T, Observers>& CE::State::ObservedVariable<T, Observers>::operator=(T value)
 * @brief Publishes through set() and returns this observer wrapper.
 * @param value New value; observers run even for an equal value.
 * @return This object; observer exceptions propagate after publication.
 */

/** @fn template<typename T, std::uint8_t Observers> void CE::State::ObservedVariable<T, Observers>::set(T value)
 * @brief Publishes the candidate, then calls each nonempty observer in array order.
 * @param value Owned argument; callbacks borrow this argument for their synchronous invocation.
 * Callbacks run on the caller after the value lock is released, even for unchanged
 * values. They can overlap/reenter with other setters and protect their own targets.
 * A callback throw skips later observers without rolling back the published value.
 * Storage comparison/assignment exceptions prevent callback execution.
 * @see [State utilities](cheryl-guide:runtime/utility-contracts)
 */

/** @fn template<typename T, std::uint8_t Observers> T CE::State::ObservedVariable<T, Observers>::get() const
 * @brief Copies the current value under the underlying storage lock.
 * @return Owned copy; it can reflect a later writer than a callback currently running.
 * Copy exceptions propagate; no observer runs.
 */

/** @fn template<typename T, std::uint8_t Observers> CE::State::VersionedSnapshot<T> CE::State::ObservedVariable<T, Observers>::snapshot() const
 * @brief Copies a consistent value/revision pair.
 * @return Owned storage snapshot, with pointee ownership determined by T.
 * No observer runs; copy failures propagate.
 */

/** @fn template<typename T, std::uint8_t Observers> std::uint64_t CE::State::ObservedVariable<T, Observers>::revision() const
 * @brief Samples the storage change generation.
 * @return Revision advanced only for a different successfully assigned value, not every callback round.
 */

/** @fn template<typename T, std::uint8_t Observers> CE::State::VersionedSnapshot<T> CE::State::ObservedVariable<T, Observers>::wait_for_change(std::uint64_t since) const
 * @brief Waits for a different revision and returns its current snapshot.
 * @param since Previously observed revision.
 * @return Owned value/revision; intermediate updates can coalesce.
 * The object must outlive waiters. No timeout/cancellation is provided; an unchanged
 * set invokes callbacks but does not wake this wait.
 */

/** @fn template<typename T, std::uint8_t Observers> void CE::State::ObservedVariable<T, Observers>::wait_until_change() const
 * @brief Waits for a change after sampling the revision at this call.
 * Changes earlier than this call are not unread events and need not wake it.
 * The object must remain alive and a later different value must be published;
 * there is no timeout or cancellation. Use wait_for_change() with a saved revision
 * when an intervening update must not be missed.
 */
