#include <core/rendering/draw-packet.h>

#include <internals/exceptions.h>

#include <utility>

namespace CE::RenderAPIs {
    void validate_draw_packet(const DrawPacket2D& packet, const Assets::PassConstraints2D& constraints) {
        if (!packet.geometry || !packet.material)
            throw Exceptions::invalid_args(CE_HERE, "A draw packet needs geometry and a material generation");
        const auto& pipeline = *packet.material->definition().pipeline;
        pipeline.validate_draw(*packet.geometry, packet.first_vertex, packet.vertex_count, constraints);
        Assets::validate_resolved_parameters(pipeline.definition().parameters, packet.parameters);
    }

    DrawPacket2D resolve_draw_packet(
        std::shared_ptr<const Assets::Geometry2D> geometry,
        const std::size_t first_vertex,
        const std::size_t vertex_count,
        const DrawStyle2D& style,
        const Assets::ShaderPass& pass,
        const Assets::ParameterSet& pass_values,
        const Assets::PassConstraints2D& constraints
    ) {
        if (!style.material)
            throw Exceptions::invalid_args(CE_HERE, "A draw submission needs a material generation");
        DrawPacket2D packet{std::move(geometry), style.material, first_vertex, vertex_count,
            style.material->resolve(pass, {style.model_matrix, style.alpha, style.scale, 0}, pass_values, style.parameters)};
        validate_draw_packet(packet, constraints);
        return packet;
    }
}
