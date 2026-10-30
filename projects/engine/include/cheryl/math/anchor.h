#pragma once

#include <cheryl/export.h>

#include <assets/types/primitives/vertex.h>

#include <cstdint>
#include <string>

namespace CE {
    namespace Rendering::Geometry {
        /** Normalized origin within a frame: (0,0) is its top-left, (1,1) its bottom-right. */
        struct Pivot {
            float x{0.5f}; ///< Normalized horizontal pivot:0 left,1 right; geometry builders require finite [0,1].
            float y{0.5f}; ///< Normalized vertical pivot:0 top,1 bottom; converted into local Y-up geometry.

            friend constexpr bool operator==(const Pivot&, const Pivot&) = default; ///< Compare x and y exactly, without tolerance or validity checks.
        };

        /** Named normalized pivot presets; unknown values resolve to Center. */
        enum AnchorType { Center, ///< (0.5,0.5).
            TopLeft, ///< (0,0).
            TopCenter, ///< (0.5,0).
            TopRight, ///< (1,0).
            CenterLeft, ///< (0,0.5).
            CenterRight, ///< (1,0.5).
            BottomLeft, ///< (0,1).
            BottomCenter, ///< (0.5,1).
            BottomRight ///< (1,1).
        };

        // Case-sensitive two-letter names (CL/ML and CR/MR are aliases); unknown
        // names or enum values fall back to Center rather than throwing.
        [[nodiscard]] CE_API AnchorType get_anchor(const std::string& anchor);
        [[nodiscard]] CE_API Pivot get_pivot(AnchorType anchor);

        /** Convert a top-left-origin image rectangle into local Y-up geometry.
         * The pointer overloads and MakeQuad write six vertices for two independent triangles;
         * MakeQuadStrip produces a separate four-vertex atlas frame.
         * CPU-only: pixel dimensions and offsets, normalized UVs, finite [0,1] pivot
         * and nonzero texture dimensions required. Source rectangle bounds/size are not checked.
         * Pointer destinations must hold six Vertex2D or thirty floats (x,y,z,u,v);
         * they are borrowed through the call. Invalid pivot/texture throws before writes.
         */
        struct Anchor {
            [[nodiscard]] CE_API static ::CE::Rendering::Geometry::Quad MakeQuad(
                ::CE::Rendering::Geometry::Pivot pivot,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            [[nodiscard]] CE_API static ::CE::Rendering::Geometry::QuadTriangleStrip MakeQuadStrip(
                ::CE::Rendering::Geometry::Pivot pivot,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            CE_API static void MakePivot(
                ::CE::Rendering::Geometry::Pivot pivot,
                float* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            CE_API static void MakePivot(
                ::CE::Rendering::Geometry::Pivot pivot,
                ::CE::Rendering::Geometry::Vertex2D* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );

            CE_API static void MakeAnchor(
                ::CE::Rendering::Geometry::AnchorType type,
                float* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            CE_API static void MakeAnchor(
                ::CE::Rendering::Geometry::AnchorType type,
                ::CE::Rendering::Geometry::Vertex2D* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );

            CE_API static void Center(
                ::CE::Rendering::Geometry::Vertex2D* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            CE_API static void TopLeft(
                ::CE::Rendering::Geometry::Vertex2D* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            CE_API static void TopRight(
                ::CE::Rendering::Geometry::Vertex2D* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            CE_API static void BottomLeft(
                ::CE::Rendering::Geometry::Vertex2D* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            CE_API static void BottomRight(
                ::CE::Rendering::Geometry::Vertex2D* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );

            CE_API static void Center(
                float* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            CE_API static void TopLeft(
                float* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            CE_API static void TopRight(
                float* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            CE_API static void BottomLeft(
                float* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            CE_API static void BottomRight(
                float* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
        };
    }
}

/**
 * @fn CE::Rendering::Geometry::AnchorType CE::Rendering::Geometry::get_anchor(const std::string& anchor)
 * @brief Translate a case-sensitive two-letter pivot name.
 * @param anchor TL,TC,TR,CL/ML,CR/MR,BL,BC,BR; other strings resolve to Center.
 * @return Named preset without throwing for an unknown name.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn CE::Rendering::Geometry::Pivot CE::Rendering::Geometry::get_pivot(AnchorType anchor)
 * @brief Convert a named preset to normalized top-left-origin coordinates.
 * @return Preset pivot; unknown enum values return the center (0.5,0.5).
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn CE::Rendering::Geometry::Quad CE::Rendering::Geometry::Anchor::MakeQuad(Pivot pivot, std::uint32_t texture_width, std::uint32_t texture_height, std::uint32_t width, std::uint32_t height, std::uint32_t x0, std::uint32_t y0)
 * @brief Return six vertices for two independent triangles around a normalized pivot.
 * @param pivot Finite components in [0,1], measured from the source frame's top-left.
 * @param texture_width Nonzero texture width in pixels.
 * @param texture_height Nonzero texture height in pixels.
 * @param width Source/frame width in pixels.
 * @param height Source/frame height in pixels.
 * @param x0 Top-left source pixel X, default0.
 * @param y0 Top-left source pixel Y, default0.
 * Source rectangle bounds/size are not checked. CPU-only output uses local Y-up pixels, z0 and normalized UVs converted from top-left source coordinates. No backend allocation or publication occurs.
 * @return Owned geometry value.
 * @throws CE::Exceptions::invalid_args For invalid pivot/zero texture dimensions, before producing output.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn CE::Rendering::Geometry::QuadTriangleStrip CE::Rendering::Geometry::Anchor::MakeQuadStrip(Pivot pivot, std::uint32_t texture_width, std::uint32_t texture_height, std::uint32_t width, std::uint32_t height, std::uint32_t x0, std::uint32_t y0)
 * @brief Return four vertices for an independent two-face strip around a normalized pivot.
 * @param pivot Finite components in [0,1], measured from the source frame's top-left.
 * @param texture_width Nonzero texture width in pixels.
 * @param texture_height Nonzero texture height in pixels.
 * @param width Source/frame width in pixels.
 * @param height Source/frame height in pixels.
 * @param x0 Top-left source pixel X, default0.
 * @param y0 Top-left source pixel Y, default0.
 * Source rectangle bounds/size are not checked. CPU-only output uses local Y-up pixels, z0 and normalized UVs converted from top-left source coordinates. No backend allocation or publication occurs.
 * @return Owned geometry value.
 * @throws CE::Exceptions::invalid_args For invalid pivot/zero texture dimensions, before producing output.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn void CE::Rendering::Geometry::Anchor::MakePivot(Pivot pivot, float* vertices, std::uint32_t texture_width, std::uint32_t texture_height, std::uint32_t width, std::uint32_t height, std::uint32_t x0, std::uint32_t y0)
 * @brief Write an independent-triangle quad into borrowed caller storage.
 * @param pivot Finite normalized top-left-origin pivot.
 * @param vertices Non-null writable storage for thirty floats in x,y,z,u,v order, borrowed only for this call.
 * @param texture_width Nonzero texture width in pixels.
 * @param texture_height Nonzero texture height in pixels.
 * @param width Source/frame width in pixels.
 * @param height Source/frame height in pixels.
 * @param x0 Top-left source pixel X, default0.
 * @param y0 Top-left source pixel Y, default0.
 * Source rectangle bounds/size are not checked. CPU-only output uses local Y-up pixels, z0 and normalized UVs converted from top-left source coordinates. No backend allocation or publication occurs.
 * @throws CE::Exceptions::invalid_args For invalid pivot/texture dimensions before any destination writes. Storage validity/capacity is the caller's precondition.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn void CE::Rendering::Geometry::Anchor::MakeAnchor(AnchorType type, float* vertices, std::uint32_t texture_width, std::uint32_t texture_height, std::uint32_t width, std::uint32_t height, std::uint32_t x0, std::uint32_t y0)
 * @brief Write the six-vertex quad for a named pivot preset.
 * @param type Preset; unknown values fall back to Center.
 * @param vertices Non-null writable storage for thirty floats in x,y,z,u,v order.
 * @param texture_width Nonzero texture width in pixels.
 * @param texture_height Nonzero texture height in pixels.
 * @param width Source/frame width in pixels.
 * @param height Source/frame height in pixels.
 * @param x0 Top-left source pixel X, default0.
 * @param y0 Top-left source pixel Y, default0.
 * Source rectangle bounds/size are not checked. CPU-only output uses local Y-up pixels, z0 and normalized UVs converted from top-left source coordinates. No backend allocation or publication occurs.
 * Zero texture dimensions reject before writes; storage validity/capacity remains the caller's responsibility.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn void CE::Rendering::Geometry::Anchor::Center(float* vertices, std::uint32_t texture_width, std::uint32_t texture_height, std::uint32_t width, std::uint32_t height, std::uint32_t x0, std::uint32_t y0)
 * @brief Write a six-vertex quad using the Center pivot preset.
 * @param vertices Non-null writable storage for thirty floats in x,y,z,u,v order, retained only by the caller.
 * @param texture_width Nonzero texture width in pixels.
 * @param texture_height Nonzero texture height in pixels.
 * @param width Source/frame width in pixels.
 * @param height Source/frame height in pixels.
 * @param x0 Top-left source pixel X, default0.
 * @param y0 Top-left source pixel Y, default0.
 * Source rectangle bounds/size are not checked. CPU-only output uses local Y-up pixels, z0 and normalized UVs converted from top-left source coordinates. No backend allocation or publication occurs.
 * Equivalent to MakeAnchor() for this preset. Zero texture dimensions reject before writes; source rectangle bounds and destination capacity are not validated.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn void CE::Rendering::Geometry::Anchor::TopLeft(float* vertices, std::uint32_t texture_width, std::uint32_t texture_height, std::uint32_t width, std::uint32_t height, std::uint32_t x0, std::uint32_t y0)
 * @brief Write a six-vertex quad using the TopLeft pivot preset.
 * @param vertices Non-null writable storage for thirty floats in x,y,z,u,v order, retained only by the caller.
 * @param texture_width Nonzero texture width in pixels.
 * @param texture_height Nonzero texture height in pixels.
 * @param width Source/frame width in pixels.
 * @param height Source/frame height in pixels.
 * @param x0 Top-left source pixel X, default0.
 * @param y0 Top-left source pixel Y, default0.
 * Source rectangle bounds/size are not checked. CPU-only output uses local Y-up pixels, z0 and normalized UVs converted from top-left source coordinates. No backend allocation or publication occurs.
 * Equivalent to MakeAnchor() for this preset. Zero texture dimensions reject before writes; source rectangle bounds and destination capacity are not validated.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn void CE::Rendering::Geometry::Anchor::TopRight(float* vertices, std::uint32_t texture_width, std::uint32_t texture_height, std::uint32_t width, std::uint32_t height, std::uint32_t x0, std::uint32_t y0)
 * @brief Write a six-vertex quad using the TopRight pivot preset.
 * @param vertices Non-null writable storage for thirty floats in x,y,z,u,v order, retained only by the caller.
 * @param texture_width Nonzero texture width in pixels.
 * @param texture_height Nonzero texture height in pixels.
 * @param width Source/frame width in pixels.
 * @param height Source/frame height in pixels.
 * @param x0 Top-left source pixel X, default0.
 * @param y0 Top-left source pixel Y, default0.
 * Source rectangle bounds/size are not checked. CPU-only output uses local Y-up pixels, z0 and normalized UVs converted from top-left source coordinates. No backend allocation or publication occurs.
 * Equivalent to MakeAnchor() for this preset. Zero texture dimensions reject before writes; source rectangle bounds and destination capacity are not validated.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn void CE::Rendering::Geometry::Anchor::BottomLeft(float* vertices, std::uint32_t texture_width, std::uint32_t texture_height, std::uint32_t width, std::uint32_t height, std::uint32_t x0, std::uint32_t y0)
 * @brief Write a six-vertex quad using the BottomLeft pivot preset.
 * @param vertices Non-null writable storage for thirty floats in x,y,z,u,v order, retained only by the caller.
 * @param texture_width Nonzero texture width in pixels.
 * @param texture_height Nonzero texture height in pixels.
 * @param width Source/frame width in pixels.
 * @param height Source/frame height in pixels.
 * @param x0 Top-left source pixel X, default0.
 * @param y0 Top-left source pixel Y, default0.
 * Source rectangle bounds/size are not checked. CPU-only output uses local Y-up pixels, z0 and normalized UVs converted from top-left source coordinates. No backend allocation or publication occurs.
 * Equivalent to MakeAnchor() for this preset. Zero texture dimensions reject before writes; source rectangle bounds and destination capacity are not validated.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn void CE::Rendering::Geometry::Anchor::BottomRight(float* vertices, std::uint32_t texture_width, std::uint32_t texture_height, std::uint32_t width, std::uint32_t height, std::uint32_t x0, std::uint32_t y0)
 * @brief Write a six-vertex quad using the BottomRight pivot preset.
 * @param vertices Non-null writable storage for thirty floats in x,y,z,u,v order, retained only by the caller.
 * @param texture_width Nonzero texture width in pixels.
 * @param texture_height Nonzero texture height in pixels.
 * @param width Source/frame width in pixels.
 * @param height Source/frame height in pixels.
 * @param x0 Top-left source pixel X, default0.
 * @param y0 Top-left source pixel Y, default0.
 * Source rectangle bounds/size are not checked. CPU-only output uses local Y-up pixels, z0 and normalized UVs converted from top-left source coordinates. No backend allocation or publication occurs.
 * Equivalent to MakeAnchor() for this preset. Zero texture dimensions reject before writes; source rectangle bounds and destination capacity are not validated.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn void CE::Rendering::Geometry::Anchor::MakePivot(Pivot pivot, Vertex2D* vertices, std::uint32_t texture_width, std::uint32_t texture_height, std::uint32_t width, std::uint32_t height, std::uint32_t x0, std::uint32_t y0)
 * @brief Write an independent-triangle quad into borrowed caller storage.
 * @param pivot Finite normalized top-left-origin pivot.
 * @param vertices Non-null writable storage for six Vertex2D values, borrowed only for this call.
 * @param texture_width Nonzero texture width in pixels.
 * @param texture_height Nonzero texture height in pixels.
 * @param width Source/frame width in pixels.
 * @param height Source/frame height in pixels.
 * @param x0 Top-left source pixel X, default0.
 * @param y0 Top-left source pixel Y, default0.
 * Source rectangle bounds/size are not checked. CPU-only output uses local Y-up pixels, z0 and normalized UVs converted from top-left source coordinates. No backend allocation or publication occurs.
 * @throws CE::Exceptions::invalid_args For invalid pivot/texture dimensions before any destination writes. Storage validity/capacity is the caller's precondition.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn void CE::Rendering::Geometry::Anchor::MakeAnchor(AnchorType type, Vertex2D* vertices, std::uint32_t texture_width, std::uint32_t texture_height, std::uint32_t width, std::uint32_t height, std::uint32_t x0, std::uint32_t y0)
 * @brief Write the six-vertex quad for a named pivot preset.
 * @param type Preset; unknown values fall back to Center.
 * @param vertices Non-null writable storage for six Vertex2D values.
 * @param texture_width Nonzero texture width in pixels.
 * @param texture_height Nonzero texture height in pixels.
 * @param width Source/frame width in pixels.
 * @param height Source/frame height in pixels.
 * @param x0 Top-left source pixel X, default0.
 * @param y0 Top-left source pixel Y, default0.
 * Source rectangle bounds/size are not checked. CPU-only output uses local Y-up pixels, z0 and normalized UVs converted from top-left source coordinates. No backend allocation or publication occurs.
 * Zero texture dimensions reject before writes; storage validity/capacity remains the caller's responsibility.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn void CE::Rendering::Geometry::Anchor::Center(Vertex2D* vertices, std::uint32_t texture_width, std::uint32_t texture_height, std::uint32_t width, std::uint32_t height, std::uint32_t x0, std::uint32_t y0)
 * @brief Write a six-vertex quad using the Center pivot preset.
 * @param vertices Non-null writable storage for six Vertex2D values, retained only by the caller.
 * @param texture_width Nonzero texture width in pixels.
 * @param texture_height Nonzero texture height in pixels.
 * @param width Source/frame width in pixels.
 * @param height Source/frame height in pixels.
 * @param x0 Top-left source pixel X, default0.
 * @param y0 Top-left source pixel Y, default0.
 * Source rectangle bounds/size are not checked. CPU-only output uses local Y-up pixels, z0 and normalized UVs converted from top-left source coordinates. No backend allocation or publication occurs.
 * Equivalent to MakeAnchor() for this preset. Zero texture dimensions reject before writes; source rectangle bounds and destination capacity are not validated.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn void CE::Rendering::Geometry::Anchor::TopLeft(Vertex2D* vertices, std::uint32_t texture_width, std::uint32_t texture_height, std::uint32_t width, std::uint32_t height, std::uint32_t x0, std::uint32_t y0)
 * @brief Write a six-vertex quad using the TopLeft pivot preset.
 * @param vertices Non-null writable storage for six Vertex2D values, retained only by the caller.
 * @param texture_width Nonzero texture width in pixels.
 * @param texture_height Nonzero texture height in pixels.
 * @param width Source/frame width in pixels.
 * @param height Source/frame height in pixels.
 * @param x0 Top-left source pixel X, default0.
 * @param y0 Top-left source pixel Y, default0.
 * Source rectangle bounds/size are not checked. CPU-only output uses local Y-up pixels, z0 and normalized UVs converted from top-left source coordinates. No backend allocation or publication occurs.
 * Equivalent to MakeAnchor() for this preset. Zero texture dimensions reject before writes; source rectangle bounds and destination capacity are not validated.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn void CE::Rendering::Geometry::Anchor::TopRight(Vertex2D* vertices, std::uint32_t texture_width, std::uint32_t texture_height, std::uint32_t width, std::uint32_t height, std::uint32_t x0, std::uint32_t y0)
 * @brief Write a six-vertex quad using the TopRight pivot preset.
 * @param vertices Non-null writable storage for six Vertex2D values, retained only by the caller.
 * @param texture_width Nonzero texture width in pixels.
 * @param texture_height Nonzero texture height in pixels.
 * @param width Source/frame width in pixels.
 * @param height Source/frame height in pixels.
 * @param x0 Top-left source pixel X, default0.
 * @param y0 Top-left source pixel Y, default0.
 * Source rectangle bounds/size are not checked. CPU-only output uses local Y-up pixels, z0 and normalized UVs converted from top-left source coordinates. No backend allocation or publication occurs.
 * Equivalent to MakeAnchor() for this preset. Zero texture dimensions reject before writes; source rectangle bounds and destination capacity are not validated.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn void CE::Rendering::Geometry::Anchor::BottomLeft(Vertex2D* vertices, std::uint32_t texture_width, std::uint32_t texture_height, std::uint32_t width, std::uint32_t height, std::uint32_t x0, std::uint32_t y0)
 * @brief Write a six-vertex quad using the BottomLeft pivot preset.
 * @param vertices Non-null writable storage for six Vertex2D values, retained only by the caller.
 * @param texture_width Nonzero texture width in pixels.
 * @param texture_height Nonzero texture height in pixels.
 * @param width Source/frame width in pixels.
 * @param height Source/frame height in pixels.
 * @param x0 Top-left source pixel X, default0.
 * @param y0 Top-left source pixel Y, default0.
 * Source rectangle bounds/size are not checked. CPU-only output uses local Y-up pixels, z0 and normalized UVs converted from top-left source coordinates. No backend allocation or publication occurs.
 * Equivalent to MakeAnchor() for this preset. Zero texture dimensions reject before writes; source rectangle bounds and destination capacity are not validated.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn void CE::Rendering::Geometry::Anchor::BottomRight(Vertex2D* vertices, std::uint32_t texture_width, std::uint32_t texture_height, std::uint32_t width, std::uint32_t height, std::uint32_t x0, std::uint32_t y0)
 * @brief Write a six-vertex quad using the BottomRight pivot preset.
 * @param vertices Non-null writable storage for six Vertex2D values, retained only by the caller.
 * @param texture_width Nonzero texture width in pixels.
 * @param texture_height Nonzero texture height in pixels.
 * @param width Source/frame width in pixels.
 * @param height Source/frame height in pixels.
 * @param x0 Top-left source pixel X, default0.
 * @param y0 Top-left source pixel Y, default0.
 * Source rectangle bounds/size are not checked. CPU-only output uses local Y-up pixels, z0 and normalized UVs converted from top-left source coordinates. No backend allocation or publication occurs.
 * Equivalent to MakeAnchor() for this preset. Zero texture dimensions reject before writes; source rectangle bounds and destination capacity are not validated.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */
