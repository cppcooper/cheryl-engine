#pragma once
#include <internals/exceptions.h>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

namespace CE::Memory::Objects {
    /**
     * Tracks raw/live slots independently of singleton teardown. Distinct slots
     * may be constructed/destroyed concurrently; overlapping operations on a
     * busy slot are rejected. Callers retain storage until their operation ends.
     * User code runs outside the tracking mutex, including reentrant operations
     * on other slots. Public static operations use the shared T context; final
     * owners capture that context and never consult static storage during release.
     */
    template <typename T> struct Constructor {
        /** Retained synchronized per-slot raw/live/busy state; user constructors/destructors run outside its mutex. */
        class Context {
            enum class Phase { raw, constructing, live, destroying };
            std::mutex mutex_;
            std::unordered_map<T*, Phase> slots_;

            Phase claim(T* pointer, bool construct) {
                Phase previous = Phase::raw;
                bool busy = false;
                {
                    std::lock_guard lock(mutex_);
                    auto entry = slots_.find(pointer);
                    if (entry == slots_.end()) {
                        if (!construct)
                            return Phase::raw;
                        entry = slots_.try_emplace(pointer, Phase::raw).first;
                    }
                    previous = entry->second;
                    busy = previous == Phase::constructing || previous == Phase::destroying;
                    if (!busy && (construct || previous == Phase::live))
                        entry->second = previous == Phase::live ? Phase::destroying : Phase::constructing;
                }
                if (busy)
                    throw ::CE::Exceptions::bad_request(CE_HERE, "Object slot already has a construction or destruction in progress.");
                return previous;
            }

            void finish(T* pointer, Phase phase) {
                std::lock_guard lock(mutex_);
                // erase() cannot remove a claimed entry. Never retain an iterator
                // across user code, which may grow the map through another slot.
                slots_.find(pointer)->second = phase;
            }

        public:
            template <typename... Args> void construct(T* pointer, std::size_t count, Args&&... args) {
                static_assert(std::is_constructible_v<T, Args...>, "No matching object constructor.");
                for (std::size_t index = 0; index < count; ++index) {
                    auto* slot = pointer + index;
                    const auto previous = claim(slot, true);
                    try {
                        if (previous == Phase::live) {
                            std::destroy_at(slot);
                            finish(slot, Phase::constructing);
                        }
                        std::construct_at(slot, std::forward<Args>(args)...);
                    } catch (...) {
                        finish(slot, Phase::raw);
                        throw;
                    }
                    finish(slot, Phase::live);
                }
            }

            void destroy(T* pointer, std::size_t count = 1) {
                for (std::size_t index = 0; index < count; ++index) {
                    auto* slot = pointer + index;
                    if (claim(slot, false) != Phase::live)
                        continue;
                    try {
                        std::destroy_at(slot);
                    } catch (...) {
                        finish(slot, Phase::raw);
                        throw;
                    }
                    finish(slot, Phase::raw);
                }
            }

            void erase(T* first, T* last) {
                bool active = false;
                {
                    std::lock_guard lock(mutex_);
                    for (auto* slot = first; slot < last; ++slot) {
                        const auto entry = slots_.find(slot);
                        if (entry != slots_.end() && entry->second != Phase::raw) {
                            active = true;
                            break;
                        }
                    }
                    if (!active) {
                        for (auto* slot = first; slot < last; ++slot)
                            slots_.erase(slot);
                    }
                }
                if (active)
                    throw ::CE::Exceptions::bad_request(CE_HERE, "Cannot erase tracking for a live or busy object slot.");
            }
        };

        [[nodiscard]] static std::shared_ptr<Context> release_context() {
            static const auto context = std::make_shared<Context>();
            return context;
        }

        template <typename... Args> static void construct(T* pointer, std::size_t count, Args... args) {
            release_context()->construct(pointer, count, std::forward<Args>(args)...);
        }
        static void destroy(T* pointer, std::size_t count = 1) { release_context()->destroy(pointer, count); }
        static void erase(T* first, T* last) { release_context()->erase(first, last); }
    };
}

/**
 * @fn template<typename T> template<typename... Args> void CE::Memory::Objects::Constructor<T>::Context::construct(T* pointer, std::size_t count, Args&&... args)
 * @brief Construct or replace slots one at a time, excluding overlapping busy-slot operations.
 * @param pointer Caller-owned aligned contiguous storage for count T objects, retained through the call.
 * @param count Number of slots; zero performs no slot operations.
 * @param args Arguments forwarded repeatedly; a move can leave later calls with moved-from values.
 * A tracked live object is destroyed before replacement. Busy slots throw bad_request; user exceptions propagate with the current slot marked raw, earlier completed slots remain live, and later slots are untouched. No batch rollback occurs. Different slots may proceed concurrently/reentrantly; accesses to object data need caller synchronization.
 */

/**
 * @fn template<typename T> void CE::Memory::Objects::Constructor<T>::Context::destroy(T* pointer, std::size_t count)
 * @brief Destroy tracked live slots one at a time, skipping raw/untracked slots.
 * @param pointer Contiguous storage kept valid through destruction.
 * @param count Slot count, one by default.
 * Overlapping construction/destruction on a busy slot throws bad_request. User destructor exceptions propagate after marking that slot raw; completed earlier destructions are not rolled back. Does not deallocate or erase tracking records.
 */

/**
 * @fn template<typename T> void CE::Memory::Objects::Constructor<T>::Context::erase(T* first, T* last)
 * @brief Forget an entirely inactive half-open slot range atomically under the tracker lock.
 * @param first First slot in comparable backing storage.
 * @param last One-past-last slot.
 * If any slot is live or busy, throws bad_request without erasing the range. Does not destroy objects or free storage; use after tracked lifetimes end.
 */

/**
 * @fn template<typename T> std::shared_ptr<typename CE::Memory::Objects::Constructor<T>::Context> CE::Memory::Objects::Constructor<T>::release_context()
 * @return Retained shared tracking context for this exact T specialization. Capture it before static/facade teardown for final-owner operations; it does not retain the underlying object storage.
 */

/**
 * @fn template<typename T> template<typename... Args> void CE::Memory::Objects::Constructor<T>::construct(T* pointer, std::size_t count, Args... args)
 * @brief Forward batch construction/replacement to the shared per-T context.
 * @param pointer Caller-owned aligned contiguous storage.
 * @param count Number of slots.
 * @param args Arguments copied/moved into this call and repeatedly forwarded to slot construction.
 * The Context per-slot synchronization, partial-effects and user-exception rules apply. This does not allocate backing or return it to a pool.
 */

/**
 * @fn template<typename T> void CE::Memory::Objects::Constructor<T>::destroy(T* pointer, std::size_t count)
 * @brief Destroy tracked live slots through the shared T context without releasing backing storage.
 * @param pointer Stable contiguous storage.
 * @param count Slot count, one by default.
 * Raw/untracked slots are skipped; busy conflicts and user exceptions propagate under Context rules.
 */

/**
 * @fn template<typename T> void CE::Memory::Objects::Constructor<T>::erase(T* first, T* last)
 * @brief Forget a raw half-open tracking range through the shared T context.
 * @param first First slot.
 * @param last One-past-last slot in the same comparable storage.
 * A live/busy range rejects atomically; no object destruction/deallocation occurs.
 * @see [CPU memory lifetime](cheryl-guide:resources/resource-lifetime)
 */
