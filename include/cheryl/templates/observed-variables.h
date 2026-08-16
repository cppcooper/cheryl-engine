#pragma once
#include <mutex>
#include <shared_mutex>
#include <condition_variable>

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
