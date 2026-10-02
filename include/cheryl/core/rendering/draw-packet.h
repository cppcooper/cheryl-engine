#pragma once

#include <assets/resources/pipeline.h>

#include <cstddef>
#include <memory>

namespace CE::RenderAPIs {
    struct DrawStyle2D {
        std::shared_ptr<const Assets::Material> material;
        glm::mat4 model_matrix{1.0f};
        float alpha = 1.0f;
        float scale = 1.0f;
        Assets::ParameterSet parameters;
    };

    /** Fully resolved CPU submission. No entity, asset, font, or live transform
     * is consulted by the renderer. Parameters own copied transforms and images.
     */
    struct DrawPacket2D {
        std::shared_ptr<const Assets::Geometry2D> geometry;
        std::shared_ptr<const Assets::Material> material;
        std::size_t first_vertex = 0;
        std::size_t vertex_count = 0;
        Assets::ParameterSet parameters;
        std::size_t authored_order = 0;
        bool order_sensitive = true;
        // TODO: derive compatibility keys from retained generations/ranges for
        // future batching. Preserve authored order until an explicit policy exists.
    };

    void validate_draw_packet(
        const DrawPacket2D& packet,
        const Assets::PassConstraints2D& constraints
    );
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
