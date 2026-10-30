#pragma once
#include <cinttypes>
#include <cstddef>
#include <cmath>
#include <limits>
#include <enums/fit-type.h>
#include <internals/exceptions.h>

namespace CE::Memory {
    /** Computes a capacity request without allocating storage.
     * @param length Requested length in caller-defined units.
     * @param fit exact/unknown retain length; larger adds gb; greedy truncates gf*length then adds gb.
     * @param gb Fixed growth margin in the same units as length.
     * @param gf Finite positive multiplier, consulted only for greedy growth.
     * @return Adjusted, representable length; it need not describe actual allocated capacity.
     * @throws CE::Exceptions::bad_request For invalid greedy factors or growth overflow.
     * @see [CPU resource lifetime](cheryl-guide:resources/resource-lifetime)
     */
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
    /** Reduces greedy to larger and every other non-greedy choice to exact.
     * @param fit Current allocation-growth choice.
     * @return Reduced policy; does not allocate or change a block.
     */
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
