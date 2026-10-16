#pragma once
#include <cinttypes>
#include <cstddef>
#include <cmath>
#include <limits>
#include <enums/fit-type.h>
#include <internals/exceptions.h>

namespace CE::Memory {
    // Caller-defined length units. larger adds gb; greedy truncates gf*length then
    // adds gb, requiring finite positive gf and representable growth. Overflow throws
    // bad_request. exact/unknown policies retain length; neither operation allocates.
    inline std::size_t adjust_length(std::size_t length, FitPolicy fit, std::size_t gb, double gf) {
        switch (fit) {
            case greedy: {
                const auto scaled = gf * static_cast<double>(length);
                const auto maximum = std::numeric_limits<std::size_t>::max();
                if (!std::isfinite(gf) || gf <= 0 || scaled >= static_cast<double>(maximum) ||
                    static_cast<std::size_t>(scaled) > maximum - gb) {
                    throw ::CE::Exceptions::bad_request(CE_HERE, "Allocation growth exceeds the representable size.");
                }
                return static_cast<std::size_t>(scaled) + gb;
            }
            case larger:
                if (length > std::numeric_limits<std::size_t>::max() - gb) {
                    throw ::CE::Exceptions::bad_request(CE_HERE, "Allocation growth exceeds the representable size.");
                }
                return length + gb;
            case exact:
            default:
                return length;
        }
    }
    // Reduce one growth tier; exact/unknown becomes exact.
    inline FitPolicy reduce_fit_policy(const FitPolicy fit) {
        switch (fit) {
            case greedy:
                return larger;
            case larger:
            case exact:
            default:
                return exact;
        }
    }
}
