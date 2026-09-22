#pragma once
#include <cinttypes>
#include <enums/fit-type.h>

namespace CE::Math {
    inline std::size_t adjust_length(std::size_t length, Enum::fitType fit, std::size_t gb, double gf) {
        switch(fit) {
            case Enum::greedy:
                return static_cast<std::size_t>(gf * static_cast<double>(length)) + gb;
            case Enum::larger:
                return length + gb;
            case Enum::exact:
                default:
                    return length;

        }
    }
    inline Enum::fitType reduce(const Enum::fitType fit) {
        switch(fit) {
            case Enum::exact:
            case Enum::larger:
                return Enum::exact;
            case Enum::greedy:
                return Enum::larger;
            default:
                return Enum::exact;
        }
    }
}
