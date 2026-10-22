#pragma once

#include <cheryl/export.h>

#include <assets/resources/pipeline.h>
#include <core/rendering/clip-region.h>

#include <cstddef>
#include <memory>
#include <optional>

namespace CE::Rendering {
    /** CPU-owned style copied during packet resolution; no entity or live transform is retained.
     * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
     */
    // CPU-owned draw inputs; resolution copies values and retains the material/images.
    // alpha/scale are shader semantics without common clamping or unit conversion.
    struct DrawStyle2D {
        std::shared_ptr<const ::CE::Rendering::Resources::Material> material; ///< Explicit material override; asset submission may select its retained default when empty.
        glm::mat4 model_matrix{1.0f}; ///< Copied local-to-world transform, identity by default.
        float alpha = 1.0f; ///< Shader alpha semantic, without common clamping.
        float scale = 1.0f; ///< Shader size semantic, without common unit conversion.
        ::CE::Rendering::Parameters::Set parameters; ///< Highest-priority copied custom values; cannot override engine semantics.
        std::optional<::CE::Rendering::ClipRegion2D> clip; ///< Optional retained logical rectangle and logical viewport extent.
    };

    /** Fully resolved CPU submission. No entity, asset, font, or live transform
     * is consulted by the renderer. Parameters own copied transforms and images.
     * Ranges use vertex indices/counts. Aggregate construction alone is unvalidated;
     * the pass writer validates before insertion and assigns authored_order.
     */
    struct DrawPacket2D {
        std::shared_ptr<const ::CE::Rendering::Geometry::Buffer2D> geometry; ///< Required retained upload; native use requires its original live domain.
        std::shared_ptr<const ::CE::Rendering::Resources::Material> material; ///< Required immutable material/pipeline generation.
        std::size_t first_vertex = 0; ///< Zero-based vertex index, not byte offset.
        std::size_t vertex_count = 0; ///< Nonzero complete-primitive vertex count checked before insertion.
        ::CE::Rendering::Parameters::Set parameters; ///< Complete copied values retaining images/samplers and resolved engine semantics.
        std::size_t authored_order = 0; ///< PassWriter assigns the packet's position in authored insertion order.
        bool order_sensitive = true; ///< Metadata only; current playback preserves authored order for every packet.
        std::optional<::CE::Rendering::ClipRegion2D> clip; ///< Logical clip copied into the draw, resolved against the playback framebuffer.
        // TODO: derive compatibility keys from retained generations/ranges for
        // future batching. Preserve authored order until an explicit policy exists.
    };

    // CPU-only validation of clip, handles, geometry range/state and complete
    // parameters. Native domain/context compatibility is checked during playback.
    CE_API void validate_draw_packet(const DrawPacket2D& packet, const ::CE::Rendering::Pipelines::PassConstraints2D& constraints);
    // Stable inputs only; throws without modifying them or publishing a frame packet.
    [[nodiscard]] CE_API DrawPacket2D resolve_draw_packet(
        std::shared_ptr<const ::CE::Rendering::Geometry::Buffer2D> geometry,
        std::size_t first_vertex,
        std::size_t vertex_count,
        const DrawStyle2D& style,
        const ::CE::Rendering::Parameters::PassSemantics& pass,
        const ::CE::Rendering::Parameters::Set& pass_values,
        const ::CE::Rendering::Pipelines::PassConstraints2D& constraints
    );
}

/**
 * @fn void CE::Rendering::validate_draw_packet(const DrawPacket2D& packet, const CE::Rendering::Pipelines::PassConstraints2D& constraints)
 * @brief Validate clip, nonnull handles, geometry range/state and complete resolved parameters on the CPU.
 * @param packet Stable retained draw values; aggregate construction does not validate them.
 * @param constraints Fixed-state requirements of its pass.
 * @throws CE::Exceptions::invalid_args For malformed clips/handles/ranges/parameters or pipeline mismatch.
 * No native domain/current-context validation or draw occurs, and inputs remain unchanged.
 */

/**
 * @fn CE::Rendering::DrawPacket2D CE::Rendering::resolve_draw_packet(std::shared_ptr<const ::CE::Rendering::Geometry::Buffer2D> geometry, std::size_t first_vertex, std::size_t vertex_count, const DrawStyle2D& style, const ::CE::Rendering::Parameters::PassSemantics& pass, const ::CE::Rendering::Parameters::Set& pass_values, const ::CE::Rendering::Pipelines::PassConstraints2D& constraints)
 * @brief Resolve and validate one independent owned CPU packet without native calls.
 * @param geometry Retained immutable upload.
 * @param first_vertex First uploaded vertex, not bytes.
 * @param vertex_count Nonzero complete-primitive range count.
 * @param style Stable draw values with a nonnull material; this primitive has no asset-default lookup.
 * @param pass Copied engine pass semantics.
 * @param pass_values Lower-priority custom pass layer.
 * @param constraints Pass-state requirements.
 * @return Complete packet retaining geometry/material/images/samplers; PassWriter later assigns authored_order.
 * Validation/allocation failures publish no packet and leave borrowed inputs unchanged.
 */
