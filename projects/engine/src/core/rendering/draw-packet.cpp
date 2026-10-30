#include <core/rendering/draw-packet.h>

#include <internals/exceptions.h>

#include <utility>

namespace CE::Rendering {
    void validate_draw_packet(const DrawPacket2D& packet, const ::CE::Rendering::Pipelines::PassConstraints2D& constraints) {
        if (packet.clip)
            validate_clip_region(*packet.clip);
        if (!packet.geometry || !packet.material)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "A draw packet needs geometry and a material generation");
        const auto& pipeline = *packet.material->definition().pipeline;
        pipeline.validate_draw(*packet.geometry, packet.first_vertex, packet.vertex_count, constraints);
        ::CE::Rendering::Parameters::validate_resolved(pipeline.definition().parameters, packet.parameters);
    }

    DrawPacket2D resolve_draw_packet(
        std::shared_ptr<const ::CE::Rendering::Geometry::Buffer2D> geometry,
        const std::size_t first_vertex,
        const std::size_t vertex_count,
        const DrawStyle2D& style,
        const ::CE::Rendering::Parameters::PassSemantics& pass,
        const ::CE::Rendering::Parameters::Set& pass_values,
        const ::CE::Rendering::Pipelines::PassConstraints2D& constraints
    ) {
        if (!style.material)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "A draw submission needs a material generation");
        DrawPacket2D packet{std::move(geometry), style.material, first_vertex, vertex_count,
            style.material->resolve(pass, {style.model_matrix, style.alpha, style.scale, 0}, pass_values, style.parameters)};
        packet.clip = style.clip;
        validate_draw_packet(packet, constraints);
        return packet;
    }
}
