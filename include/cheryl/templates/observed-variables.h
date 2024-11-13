#pragma once
#ifndef OBSERVED_VARIABLES_H
#define OBSERVED_VARIABLES_H
#include <mutex>
#include <shared_mutex>
#include <condition_variable>

template<typename T, uint8_t Observers = 1>
struct ObservedVariable {
    static_assert(Observers != 0, "The number of observers is not allowed to be less than 1.");
    using Callback = void(*)(const T&);
    explicit ObservedVariable(T v, std::array<Callback, Observers> callbacks) : var(std::move(v)), callbacks(std::move(callbacks)) {}

    ObservedVariable& operator=(T v) { set(v); return *this; }
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
        for(auto callback : callbacks) {
            if (callback) [[likely]] {
                callback(var);
            }
        }
    }
    [[nodiscard]] T get() const {
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
protected:
    uint64_t q = 0;
    T var;
    std::shared_mutex mtx;
    std::condition_variable_any cv;
    std::array<Callback, Observers> callbacks;
};

#endif //OBSERVED_VARIABLES_H
