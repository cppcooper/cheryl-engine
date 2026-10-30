#pragma once

#include <cheryl/export.h>

#include <array>
#include <assets/resources/geometry2d.h>
#include <assets/types/primitives/vertex.h>
#include <backends/opengl/gl.h>
#include <backends/opengl/resource-lifetime.h>
#include <cstdint>
#include <memory>
#include <span>
#include <cstddef>

namespace CE::Rendering::OpenGL {
    /** Immutable native vertex upload retaining its VAO and buffers.
     * 2D ranges use vertex counts, not bytes. Indexed 3D construction is supported
     * as native storage but reports Unsupported layout and rejects the 2D draw path.
     * CPU upload views/owners are not retained after construction.
     * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
     */
    struct CE_TYPE_API VertexArray final : ::CE::Rendering::Geometry::Buffer2D {
    protected:
        /** Protected upload family; the common 2D draw path accepts flat only. */
        enum VAOType {
            flat, ///< Nonindexed position/UV or position/UV/color geometry.
            mesh ///< Indexed position/normal/UV storage, outside common 2D playback.
        } type; ///< Upload family selected during construction.

        ::CE::Rendering::Geometry::Topology topology_ = ::CE::Rendering::Geometry::Topology::Triangles; ///< Uploaded primitive interpretation.
        ::CE::Rendering::Geometry::Layout2D layout_ = ::CE::Rendering::Geometry::Layout2D::Position3UV2; ///< Selected flat vertex layout; mesh reports Unsupported.
        std::size_t vertex_count_ = 0; ///< Number of uploaded vertices, independent of mesh index count.

        ::CE::Rendering::OpenGL::Handle vao_; ///< Retained vertex-array registration in the original native domain.
        std::array<::CE::Rendering::OpenGL::Handle, 2> vbo_; ///< Retained native index/vertex buffers; final release queues retirement.

    public:
        CE_OPENGL_API VertexArray(
            std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> lifetime,
            std::span<const ::CE::Rendering::Geometry::Vertex2D> vertices,
            ::CE::Rendering::Geometry::Topology topology
        );
        CE_OPENGL_API VertexArray(
            std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> lifetime,
            std::span<const ::CE::Rendering::Geometry::Vertex2DColor> vertices,
            ::CE::Rendering::Geometry::Topology topology
        );
        CE_OPENGL_API VertexArray(
            std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> lifetime,
            std::shared_ptr<::CE::Rendering::Geometry::Vertex2D> vertices,
            uint32_t num_vertices,
            ::CE::Rendering::Geometry::Topology topology
        );

        CE_OPENGL_API VertexArray(
            std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> lifetime,
            std::shared_ptr<::CE::Rendering::Geometry::Vertex3D> vertices,
            uint32_t num_vertices,
            std::shared_ptr<uint32_t> indices,
            uint32_t num_indices
        );

        CE_OPENGL_API void bind() const override;
        CE_OPENGL_API void draw(std::size_t first_vertex, std::size_t vertex_count) const override;
        [[nodiscard]] ::CE::Rendering::Geometry::Layout2D vertex_layout() const noexcept override {
            return type == flat ? layout_ : ::CE::Rendering::Geometry::Layout2D::Unsupported;
        }
        [[nodiscard]] ::CE::Rendering::Geometry::Topology topology() const noexcept override { return topology_; }
        [[nodiscard]] std::size_t vertex_count() const noexcept override { return vertex_count_; }
        [[nodiscard]] const ::CE::Rendering::OpenGL::ResourceLifetime* resource_domain() const noexcept { return vao_.resource_domain(); }
        CE_OPENGL_API void require_draw(std::size_t first_vertex, std::size_t vertex_count) const;

    private:
        CE_OPENGL_API void upload_flat(
            const std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime>& lifetime,
            const void* vertices,
            std::size_t byte_stride
        );
    };
}

/**
 * @fn CE::Rendering::OpenGL::VertexArray::VertexArray(std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> lifetime, std::span<const ::CE::Rendering::Geometry::Vertex2D> vertices, ::CE::Rendering::Geometry::Topology topology)
 * @brief Upload position3/UV2 vertices with fixed common attribute locations.
 * @param lifetime Required live owner/current-context native domain.
 * @param vertices Nonempty contiguous CPU data within GLsizei/GLsizeiptr limits, borrowed until return.
 * @param topology Triangles or TriangleStrip; no per-primitive completeness check is made until packet validation.
 * Malformed input/topology throws invalid_args. Native/allocation failures retire completed adopted objects; construction may change native bindings and does not restore their prior values.
 */

/**
 * @fn CE::Rendering::OpenGL::VertexArray::VertexArray(std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> lifetime, std::span<const ::CE::Rendering::Geometry::Vertex2DColor> vertices, ::CE::Rendering::Geometry::Topology topology)
 * @brief Upload position3/UV2/color4 vertices with fixed common attribute locations.
 * @param lifetime Required live owner/current-context native domain.
 * @param vertices Nonempty contiguous CPU data within GLsizei/GLsizeiptr limits, borrowed until return.
 * @param topology Triangles or TriangleStrip; no per-primitive completeness check is made until packet validation.
 * Malformed input/topology throws invalid_args. Native/allocation failures retire completed adopted objects; construction may change native bindings and does not restore their prior values.
 */

/**
 * @fn CE::Rendering::OpenGL::VertexArray::VertexArray(std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> lifetime, std::shared_ptr<::CE::Rendering::Geometry::Vertex2D> vertices, uint32_t num_vertices, ::CE::Rendering::Geometry::Topology topology)
 * @brief Compatibility upload retaining the CPU array only through the span constructor.
 * @param lifetime Required live native domain.
 * @param vertices Owner of at least num_vertices contiguous vertices.
 * @param num_vertices Nonzero count, not bytes.
 * @param topology Uploaded triangle/strip interpretation.
 * Null/empty input rejects; remaining size/native/failure behavior matches the position/UV span overload.
 */

/**
 * @fn CE::Rendering::OpenGL::VertexArray::VertexArray(std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> lifetime, std::shared_ptr<::CE::Rendering::Geometry::Vertex3D> vertices, uint32_t num_vertices, std::shared_ptr<uint32_t> indices, uint32_t num_indices)
 * @brief Copy an indexed 3D mesh into native vertex/element buffers.
 * @param lifetime Required live owner/current-context native domain.
 * @param vertices Contiguous position/normal/UV array borrowed through upload.
 * @param num_vertices Positive addressable vertex count.
 * @param indices Contiguous unsigned index array borrowed through upload.
 * @param num_indices Positive addressable index count.
 * Null/empty/oversized buffers throw invalid_args; index contents are not bounds-validated here. The common 2D draw() path rejects meshes; construction retains no CPU owners and may change native bindings.
 */

/**
 * @fn void CE::Rendering::OpenGL::VertexArray::bind() const
 * @brief Bind this VAO after requiring its original live owner/current context; no program, image or material is selected.
 */

/**
 * @fn void CE::Rendering::OpenGL::VertexArray::require_draw(std::size_t first_vertex, std::size_t vertex_count) const
 * @brief Validate live owner/current context, flat upload kind and bounded range without binding/drawing.
 * @param first_vertex First vertex index.
 * @param vertex_count Range count; zero/primitive completeness is checked by common pipeline validation, not this low-level check.
 * Mesh/range mismatch throws invalid_args; lifetime failures throw failed_operation.
 */

/**
 * @fn void CE::Rendering::OpenGL::VertexArray::draw(std::size_t first_vertex, std::size_t vertex_count) const
 * @brief Issue glDrawArrays for this flat upload's topology after require_draw().
 * @param first_vertex First vertex index, not a byte offset.
 * @param vertex_count Bounded vertex count representable by the uploaded native limits.
 * Caller must bind this VAO and compatible program/material state first. This primitive does not bind it or enforce nonzero/complete primitives; pipeline playback does.
 */

/**
 * @fn CE::Rendering::Geometry::Layout2D CE::Rendering::OpenGL::VertexArray::vertex_layout() const noexcept
 * @return Uploaded flat layout or Unsupported for indexed mesh construction; CPU metadata only.
 */

/**
 * @fn CE::Rendering::Geometry::Topology CE::Rendering::OpenGL::VertexArray::topology() const noexcept
 * @return Uploaded topology metadata without native calls.
 */

/**
 * @fn std::size_t CE::Rendering::OpenGL::VertexArray::vertex_count() const noexcept
 * @return Uploaded vertex count, not bytes or mesh index count; CPU inspection only.
 */

/**
 * @fn const CE::Rendering::OpenGL::ResourceLifetime* CE::Rendering::OpenGL::VertexArray::resource_domain() const noexcept
 * @return Borrowed original native-domain identity while retaining this upload; no current/active validation occurs.
 */
