#pragma once

#include "anchor.h"
#include <glm/vec2.hpp>

namespace CE::Math {
    /** Test whether two axis-aligned rectangles share positive area in Y-up coordinates.
     * @param p1 Position of the first rectangle's anchor, in caller-defined units.
     * @param p2 Position of the second rectangle's anchor, in the same coordinates and units.
     * @param d1 Full width and height of the first rectangle; nonpositive dimensions return false.
     * @param d2 Full width and height of the second rectangle; nonpositive dimensions return false.
     * @param a1 Normalized top-left-origin anchor of the first rectangle; defaults to {0.5,0.5} (center).
     * @param a2 Normalized top-left-origin anchor of the second rectangle; defaults to {0.5,0.5} (center).
     * @return True for overlap or containment; false for separation, touching edges/corners or empty rectangles.
     * @pre Positions and dimensions are finite, anchors are finite and in [0,1], and calculated edges are representable.
     * Rotation, scaling and coordinate conversion are the caller's responsibility. No allocation or renderer is required.
     * @see [Rectangle overlap](cheryl-guide:runtime/utility-contracts#test-rectangle-overlap)
     */
    [[nodiscard]] constexpr bool overlaps(
        const glm::vec2 p1,
        const glm::vec2 p2,
        const glm::vec2 d1,
        const glm::vec2 d2,
        const ::CE::Rendering::Geometry::Pivot a1 = ::CE::Rendering::Geometry::Pivot{},
        const ::CE::Rendering::Geometry::Pivot a2 = ::CE::Rendering::Geometry::Pivot{}
    ) noexcept {
        if (d1.x <= 0.0f || d1.y <= 0.0f || d2.x <= 0.0f || d2.y <= 0.0f)
            return false;

        const auto left1 = p1.x - a1.x * d1.x;
        const auto right1 = p1.x + (1.0f - a1.x) * d1.x;
        const auto bottom1 = p1.y - (1.0f - a1.y) * d1.y;
        const auto top1 = p1.y + a1.y * d1.y;
        const auto left2 = p2.x - a2.x * d2.x;
        const auto right2 = p2.x + (1.0f - a2.x) * d2.x;
        const auto bottom2 = p2.y - (1.0f - a2.y) * d2.y;
        const auto top2 = p2.y + a2.y * d2.y;
        return left1 < right2 && left2 < right1 && bottom1 < top2 && bottom2 < top1;
    }
}
