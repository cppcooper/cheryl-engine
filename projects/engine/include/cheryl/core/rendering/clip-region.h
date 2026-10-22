#pragma once

#include <cheryl/export.h>

#include <core/display/framebuffer-size.h>

namespace CE::Rendering {
    /** Axis-aligned logical edges, top-left origin; equal edges are empty, inverted/nonfinite edges invalid. */
    // Axis-aligned edges in logical coordinates, with the origin at the top left.
    // Equal edges describe an empty rectangle; inverted/non-finite edges are invalid.
    struct ClipRect2D {
        double left = 0; ///< Inclusive left logical edge.
        double top = 0; ///< Inclusive top logical edge.
        double right = 0; ///< Exclusive right logical edge.
        double bottom = 0; ///< Exclusive bottom logical edge.
        bool operator==(const ClipRect2D&) const = default;
    };

    // Copy the logical extent with the rectangle into the retained draw. Playback
    // maps it to the current framebuffer without consulting a live UI or window.
    /** Retained logical clip plus its logical viewport, independent of a live UI/window. */
    struct ClipRegion2D {
        ::CE::Rendering::ClipRect2D rectangle; ///< Finite ordered logical edges, clamped during framebuffer resolution.
        double logical_width = 0; ///< Finite positive logical viewport width; required even for an empty rectangle.
        double logical_height = 0; ///< Finite positive logical viewport height.
        bool operator==(const ClipRegion2D&) const = default;
    };

    /** Integer framebuffer edges with top-left origin and exclusive right/bottom edges. */
    struct PixelClipRect2D {
        int left = 0; ///< Inclusive left framebuffer edge, in pixels.
        int top = 0; ///< Inclusive top framebuffer edge, in pixels.
        int right = 0; ///< Exclusive right framebuffer edge, in pixels.
        int bottom = 0; ///< Exclusive bottom framebuffer edge, in pixels.

        [[nodiscard]] bool empty() const { return left == right || top == bottom; }
        bool operator==(const PixelClipRect2D&) const = default;
    };

    CE_API void validate_clip_region(const ClipRegion2D& region);
    [[nodiscard]] CE_API ClipRect2D intersect_clip_rects(const ClipRect2D& first, const ClipRect2D& second);
    // Clamp to the logical viewport, then round nonempty edges outwards: floor
    // left/top, ceil right/bottom. Empty clips and zero-sized framebuffers draw nothing.
    [[nodiscard]] CE_API PixelClipRect2D resolve_clip_region(const ClipRegion2D& region, ::CE::Platform::FramebufferSize framebuffer);
}

/**
 * @fn bool CE::Rendering::PixelClipRect2D::empty() const
 * @return True when either dimension is zero; this predicate does not validate inverted manually constructed rectangles.
 */

/**
 * @fn void CE::Rendering::validate_clip_region(const ClipRegion2D& region)
 * @brief Validate finite ordered edges and a finite positive logical viewport.
 * @param region Stable CPU region; equal edges are valid empty rectangles.
 * @throws CE::Exceptions::invalid_args For inverted/nonfinite edges or nonpositive/nonfinite viewport dimensions.
 * No clamping, native scissor update or mutation occurs.
 */

/**
 * @fn CE::Rendering::ClipRect2D CE::Rendering::intersect_clip_rects(const ClipRect2D& first, const ClipRect2D& second)
 * @brief Intersect finite ordered rectangles in one common logical coordinate space.
 * @param first First rectangle, borrowed through the call.
 * @param second Second rectangle in the same units/origin.
 * @return Ordered intersection, with equal edges when disjoint.
 * Invalid input throws invalid_args; no viewport conversion occurs.
 */

/**
 * @fn CE::Rendering::PixelClipRect2D CE::Rendering::resolve_clip_region(const ClipRegion2D& region, CE::Platform::FramebufferSize framebuffer)
 * @brief Clamp to the logical viewport and map outward-rounded edges to framebuffer pixels.
 * @param region Retained logical rectangle and positive logical extent.
 * @param framebuffer Current nonnegative pixel width/height.
 * @return Top-left pixel bounds: floor left/top, ceil right/bottom; empty input or zero framebuffer returns an empty rectangle.
 * Invalid region or negative framebuffer dimensions throw invalid_args. CPU-only: no native scissor/bind occurs.
 * @see [Clipping](cheryl-guide:rendering/pipelines-and-materials#rectangular-clipping)
 */

/**
 * @fn bool CE::Rendering::ClipRect2D::operator==(const ClipRect2D&) const
 * @brief Compare all stored fields exactly without validating dimensions or resolving coordinates. Floating-point fields retain ordinary C++ equality semantics where present.
 */

/**
 * @fn bool CE::Rendering::ClipRegion2D::operator==(const ClipRegion2D&) const
 * @brief Compare all stored fields exactly without validating dimensions or resolving coordinates. Floating-point fields retain ordinary C++ equality semantics where present.
 */

/**
 * @fn bool CE::Rendering::PixelClipRect2D::operator==(const PixelClipRect2D&) const
 * @brief Compare all stored fields exactly without validating dimensions or resolving coordinates. Floating-point fields retain ordinary C++ equality semantics where present.
 */
