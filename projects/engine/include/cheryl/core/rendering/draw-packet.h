#pragma once

#include <assets/resources/pipeline.h>
#include <core/rendering/clip-region.h>

#include <cstddef>
#include <memory>
#include <optional>

namespace CE::RenderAPIs {
    // CPU-owned draw inputs; resolution copies values and retains the material/images.
    // alpha/scale are shader semantics without common clamping or unit conversion.
    struct DrawStyle2D {
        std::shared_ptr<const Assets::Material> material;
        glm::mat4 model_matrix{1.0f};
        float alpha = 1.0f;
        float scale = 1.0f;
        Assets::ParameterSet parameters;
        std::optional<ClipRegion2D> clip;
    };

    /** Fully resolved CPU submission. No entity, asset, font, or live transform
     * is consulted by the renderer. Parameters own copied transforms and images.
     * Ranges use vertex indices/counts. Aggregate construction alone is unvalidated;
     * the pass writer validates before insertion and assigns authored_order.
     */
    struct DrawPacket2D {
        std::shared_ptr<const Assets::Geometry2D> geometry;
        std::shared_ptr<const Assets::Material> material;
        std::size_t first_vertex = 0;
        std::size_t vertex_count = 0;
        Assets::ParameterSet parameters;
        std::size_t authored_order = 0;
        bool order_sensitive = true; // Metadata only; current playback preserves all authored order.
        std::optional<ClipRegion2D> clip;
        // TODO: derive compatibility keys from retained generations/ranges for
        // future batching. Preserve authored order until an explicit policy exists.
    };

    // CPU-only validation of clip, handles, geometry range/state and complete
    // parameters. Native domain/context compatibility is checked during playback.
    void validate_draw_packet(const DrawPacket2D& packet, const Assets::PassConstraints2D& constraints);
    // Stable inputs only; throws without modifying them or publishing a frame packet.
    [[nodiscard]] DrawPacket2D resolve_draw_packet(
        std::shared_ptr<const Assets::Geometry2D> geometry,
        std::size_t first_vertex,
        std::size_t vertex_count,
        const DrawStyle2D& style,
        const Assets::ShaderPass& pass,
        const Assets::ParameterSet& pass_values,
        const Assets::PassConstraints2D& constraints
    );
}
