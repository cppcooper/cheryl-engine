#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <utility>

/** Store a value with a change counter for waiters and an array of callbacks.
 * A set() call invokes callbacks even when the value compares equal; only an
 * actual change increments the counter and wakes wait_until_change().
 */
template<typename T, uint8_t Observers = 1>
struct ObservedVariable {
    using Callback = std::function<void(const T&)>;

protected:
    uint64_t q = 0;
    T var;
    std::shared_mutex mtx;
    std::condition_variable_any cv;
    std::array<Callback, Observers> callbacks;

public:
    explicit ObservedVariable(T v, std::array<Callback, Observers> callbacks) : var(std::move(v)), callbacks(std::move(callbacks)) {}

    ObservedVariable& operator=(T v) { set(v); return *this; }
    // TODO: Invoke callbacks after releasing mtx and pass a stable value snapshot. A callback that calls set()
    // on this object can otherwise deadlock trying to acquire the unique lock while the callback invocation
    // still holds a shared lock; long callbacks also unnecessarily block writers.
    void set(T v) {
        std::unique_lock wl(mtx);
        if (var != v) {
            var = std::move(v);
            q++;
            cv.notify_all();
        }
        wl.unlock();
        wl.release();
        std::shared_lock rl(mtx);
        for(auto &callback : callbacks) {
            if (callback) [[likely]] {
                callback(var);
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
        while(true) {
            cv.wait(lock);
            if (ov != q) break;
        }
    }
};
