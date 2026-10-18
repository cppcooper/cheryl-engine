#pragma once

#include <cheryl/export.h>

#include <cstddef>

namespace CE::Rendering::Geometry {
    /** Interpretation of a submitted contiguous vertex range. */
    enum class Topology {
        Triangles, ///< Independent groups of three vertices.
        TriangleStrip ///< One connected strip; a draw needs at least three vertices.
    };
    /** CPU metadata identifying the uploaded vertex layout. */
    enum class Layout2D {
        Position3UV2, ///< Three position floats followed by two texture-coordinate floats.
        Position3UV2Color4, ///< Position/UV followed by four floating RGBA components.
        Unsupported ///< Geometry outside the common 2D pipeline layouts.
    };

    /** Retained immutable vertex upload, independent of material/image bindings.
     * Metadata access requires no native binding; draw operations require the live backend.
     * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
     */
    // Retained immutable upload, independent of material/image bindings. Logical
    // ownership can outlive shutdown; native operations require its live backend domain.
    struct CE_API Buffer2D {
        virtual ~Buffer2D() = default;
        // Immutable upload metadata; safe to inspect during CPU frame preparation.
        [[nodiscard]] virtual Layout2D vertex_layout() const noexcept = 0;
        [[nodiscard]] virtual Topology topology() const noexcept = 0;
        [[nodiscard]] virtual std::size_t vertex_count() const noexcept = 0;
        // Bind/draw obey backend owner/current-context rules and do not select a material.
        virtual void bind() const = 0;
        // Range uses vertex indices/counts, not bytes; draw one complete primitive range
        // using the uploaded topology. Backend-specific state/validation still applies.
        virtual void draw(std::size_t first_vertex, std::size_t vertex_count) const = 0;
    };
}

/**
 * @fn CE::Rendering::Geometry::Buffer2D::~Buffer2D()
 * @brief Release logical geometry ownership; native destruction or deferred retirement belongs to the backend.
 */

/**
 * @fn CE::Rendering::Geometry::Layout2D CE::Rendering::Geometry::Buffer2D::vertex_layout() const noexcept
 * @return Immutable layout metadata used by CPU pipeline validation; no backend bind occurs.
 */

/**
 * @fn CE::Rendering::Geometry::Topology CE::Rendering::Geometry::Buffer2D::topology() const noexcept
 * @return Primitive interpretation selected at upload; a draw does not override it.
 */

/**
 * @fn std::size_t CE::Rendering::Geometry::Buffer2D::vertex_count() const noexcept
 * @return Total uploaded vertices, measured in vertices rather than bytes or primitives.
 */

/**
 * @fn void CE::Rendering::Geometry::Buffer2D::bind() const
 * @brief Bind uploaded geometry on its backend owner/current context.
 * The caller retains geometry and its live domain; binding does not select a program, material or image.
 */

/**
 * @fn void CE::Rendering::Geometry::Buffer2D::draw(std::size_t first_vertex, std::size_t vertex_count) const
 * @brief Draw a contiguous range using this upload's topology.
 * @param first_vertex Zero-based first vertex, not a byte offset.
 * @param vertex_count Number of vertices; the range must fit and contain complete primitives.
 * Requires the backend owner/current context and compatible draw state. Concrete backends perform native/domain validation and may throw; this interface does not publish a frame.
 */
