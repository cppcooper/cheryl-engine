#pragma once
#include <cinttypes>
#include <cstddef>
#include <cmath>
#include <limits>
#include <enums/fit-type.h>
#include <internals/exceptions.h>

namespace CE::Math {
    inline std::size_t adjust_length(std::size_t length, Enum::fitType fit, std::size_t gb, double gf) {
        switch (fit) {
            case Enum::greedy: {
                const auto scaled = gf * static_cast<double>(length);
                const auto maximum = std::numeric_limits<std::size_t>::max();
                if (!std::isfinite(gf) || gf <= 0 || scaled >= static_cast<double>(maximum) ||
                    static_cast<std::size_t>(scaled) > maximum - gb) {
                    throw Exceptions::bad_request(CE_HERE, "Allocation growth exceeds the representable size.");
                }
                return static_cast<std::size_t>(scaled) + gb;
            }
            case Enum::larger:
                if (length > std::numeric_limits<std::size_t>::max() - gb) {
                    throw Exceptions::bad_request(CE_HERE, "Allocation growth exceeds the representable size.");
                }
                return length + gb;
            case Enum::exact:
            default:
                return length;
        }
    }
    inline Enum::fitType reduce(const Enum::fitType fit) {
        switch (fit) {
            case Enum::greedy:
                return Enum::larger;
            case Enum::larger:
            case Enum::exact:
            default:
                return Enum::exact;
        }
    }
}
