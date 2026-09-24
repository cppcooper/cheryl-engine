#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <type_traits>
#include <utility>

/** Store a value with a change counter for waiters and an array of callbacks.
 * A set() call invokes callbacks even when the value compares equal; only an
 * actual change increments the counter and wakes wait_until_change().
 */
template<typename T, uint8_t Observers = 1>
struct ObservedVariable {
    static_assert(
        std::is_copy_constructible_v<T>,
        "ObservedVariable<T> requires T to be copy constructible."
    );

    using Callback = std::function<void(const T&)>;

protected:
    uint64_t q = 0;
    T var;
    std::shared_mutex mtx;
    std::condition_variable_any cv;
    std::array<Callback, Observers> callbacks;

public:
    explicit ObservedVariable(T v, std::array<Callback, Observers> callbacks) : var(std::move(v)), callbacks(std::move(callbacks)) {}

    ObservedVariable& operator=(T v) {
        set(std::move(v));
        return *this;
    }
    void set(T v) {
        // Only a changed value advances the sequence that waiters observe;
        // callbacks below still receive the current value on every set().
        std::unique_lock wl(mtx);
        if (var != v) {
            var = std::move(v);
            q++;
            cv.notify_all();
        }
        T snapshot{var};
        wl.unlock();
        // Invoke observers after releasing the lock, using the snapshot to keep
        // this set()'s value stable even if another writer changes var concurrently.
        for(auto &callback : callbacks) {
            if (callback) [[likely]] {
                callback(snapshot);
            }
        }
    }
    [[nodiscard]] T get() {
        std::shared_lock rl(mtx);
        return var;
    }
    void wait_until_change() {
        std::shared_lock lock(mtx);
        auto ov = q;
        // Recheck the saved sequence after wakeup: a notification alone does
        // not prove that this value changed while the caller was waiting.
        while(true) {
            cv.wait(lock);
            if (ov != q) break;
        }
    }
};
