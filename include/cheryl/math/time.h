#pragma once
#include <chrono>
#include <type_traits>

using t_elapsed = std::chrono::duration<signed long int, std::ratio<1, 1000000000>>;
using Hours = std::chrono::hours;
using Minutes = std::chrono::minutes;
using Seconds = std::chrono::seconds;
using Milliseconds = std::chrono::milliseconds;

template <typename T>
struct is_chrono_duration : std::false_type {};

template <typename Rep, typename Period>
struct is_chrono_duration<std::chrono::duration<Rep, Period>> : std::true_type {};

template <typename T>
constexpr bool is_chrono_duration_v = is_chrono_duration<T>::value;

template<typename In, typename Out>
Out tcast(const In& t) {
    static_assert(is_chrono_duration_v<Out>, "tcast can only cast durations to other durations");
    static_assert(is_chrono_duration_v<In>, "tcast can only cast durations to other durations");
    return std::chrono::duration_cast<Out>(t);
}

template<typename In>
Hours hcast(const In& t) {
    return tcast<In,Hours>(t);
}

template<typename In>
Minutes mcast(const In& t) {
    return tcast<In,Minutes>(t);
}

template<typename In>
Seconds scast(const In& t) {
    return tcast<In,Seconds>(t);
}

template<typename In>
Milliseconds mscast(const In& t) {
    return tcast<In,Milliseconds>(t);
}

