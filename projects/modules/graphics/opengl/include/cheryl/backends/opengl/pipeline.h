#pragma once

#include <assets/resources/pipeline.h>
#include <core/rendering/clip-region.h>
#include <backends/opengl/glslprogram.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <cstddef>

namespace CE::Rendering::OpenGL {
    struct ParameterBinding {
        std::string key;
        std::string uniform;
        // Optional active custom uniforms require this or a contract default.
        // A missing engine semantic instead rejects the incomplete packet.
        std::optional<CE::Rendering::Parameters::Value> missing_value;
    };

    struct PipelineBindings {
        std::vector<ParameterBinding> parameters;
        std::string position_attribute = "in_Position";
        std::string uv_attribute = "in_Texcoord";
        std::string color_attribute = "in_Color";
    };

    class Pipeline final : public CE::Rendering::Resources::Pipeline {
        struct BoundParameter {
            std::string key;
            GLint location = -1;
            std::optional<CE::Rendering::Parameters::Value> missing_value;
        };

        const std::shared_ptr<Program> program_;
        std::vector<BoundParameter> parameters_;

    public:
        Pipeline(CE::Rendering::Pipelines::Definition definition, std::shared_ptr<Program> program, const PipelineBindings& bindings);
        [[nodiscard]] const ResourceLifetime* resource_domain() const noexcept { return program_->resource_domain(); }
        // Check images/defaults without binding or changing any native state.
        void validate_resources(const CE::Rendering::Parameters::Set& values) const;
        // Applies copied parameter/resource values only; fixed state/pass integration is separate.
        void bind_parameters(const CE::Rendering::Parameters::Set& values) const;
        // Validate the complete request before mutating any native draw state.
        void draw(
            const CE::Rendering::Geometry::Buffer2D& geometry,
            std::size_t first_vertex,
            std::size_t vertex_count,
            const CE::Rendering::Parameters::Set& values,
            const CE::Rendering::Pipelines::PassConstraints2D& constraints
        ) const;
        // A clipped draw supplies its playback extent explicitly.
        void draw(
            const CE::Rendering::Geometry::Buffer2D& geometry,
            std::size_t first_vertex,
            std::size_t vertex_count,
            const CE::Rendering::Parameters::Set& values,
            const CE::Rendering::Pipelines::PassConstraints2D& constraints,
            const std::optional<ClipRegion2D>& clip,
            CE::Platform::FramebufferSize framebuffer
        ) const;

    private:
        [[nodiscard]] CE::Rendering::Parameters::Set prepare_parameters(const CE::Rendering::Parameters::Set& values) const;
        void apply_parameters(const CE::Rendering::Parameters::Set& values) const;
        void apply_fixed_state() const;
    };
}
