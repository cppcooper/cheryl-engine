#pragma once
#include <cinttypes>
#include <cstddef>
#include <cmath>
#include <limits>
#include <enums/fit-type.h>
#include <internals/exceptions.h>

namespace CE::Math {
    // Caller-defined length units. larger adds gb; greedy truncates gf*length then
    // adds gb, requiring finite positive gf and representable growth. Overflow throws
    // bad_request. exact/unknown policies retain length; neither operation allocates.
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
    // Reduce one growth tier; exact/unknown becomes exact.
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
