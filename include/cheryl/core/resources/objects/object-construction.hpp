#pragma once
#include <internals/exceptions.h>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

namespace CE::Obj {
    /**
     * Tracks raw/live slots independently of singleton teardown. Distinct slots
     * may be constructed/destroyed concurrently; overlapping operations on a
     * busy slot are rejected. Callers retain storage until their operation ends.
     * User code runs outside the tracking mutex, including reentrant operations
     * on other slots. Public static operations use the shared T context; final
     * owners capture that context and never consult static storage during release.
     */
    template <typename T> struct ObjCtor {
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
                    throw Exceptions::bad_request(CE_HERE, "Object slot already has a construction or destruction in progress.");
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

            /** Forget only inactive slots; a live or busy range remains unchanged. */
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
                    throw Exceptions::bad_request(CE_HERE, "Cannot erase tracking for a live or busy object slot.");
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
