#include <backends/opengl/resource-provider.h>

#include <backends/opengl/renderer.h>
#include <internals/exceptions.h>

#include <string>
#include <utility>

namespace CE::Rendering::OpenGL {
    namespace {
        constexpr auto vertex_source = R"GLSL(#version 330 core
uniform mat4 projectionMatrix;
layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec2 in_Texcoord;
layout(location = 2) in vec4 in_Color;
out vec2 v_texcoord;
out vec4 v_color;
void main() {
    gl_Position = projectionMatrix * vec4(in_Position, 1.0);
    v_texcoord = in_Texcoord;
    v_color = in_Color;
}
)GLSL";

        constexpr auto solid_source = R"GLSL(#version 330 core
in vec4 v_color;
out vec4 out_Color;
void main() {
    out_Color = v_color;
}
)GLSL";

        constexpr auto textured_source = R"GLSL(#version 330 core
uniform sampler2D mytexture;
in vec2 v_texcoord;
in vec4 v_color;
out vec4 out_Color;
void main() {
    out_Color = texture(mytexture, v_texcoord) * v_color;
}
)GLSL";
    }

    std::shared_ptr<const CE::Rendering::Resources::Material>
    ResourceProvider::build_builtin_material(const CE::Rendering::Materials::Builtin2D kind, const CE::Rendering::Pipelines::Blend blend) {
        // A warm cache must still reject a stopped domain or the wrong owner.
        static_cast<void>(renderer_.resources());
        const auto key = std::pair{kind, blend};
        if (const auto cached = builtin_materials_.find(key); cached != builtin_materials_.end())
            if (auto material = cached->second.lock())
                return material;

        bool textured;
        switch (kind) {
            case CE::Rendering::Materials::Builtin2D::SolidColor:
                textured = false;
                break;
            case CE::Rendering::Materials::Builtin2D::TexturedColor:
                textured = true;
                break;
            default:
                throw Exceptions::invalid_args(CE_HERE, "Unknown built-in 2D material");
        }
        const std::string name = textured ? "textured-color-2d" : "solid-color-2d";
        const CE::Assets::Shaders::ProgramRecipe program_recipe{
            "cheryl.opengl:" + name,
            {{CE::Assets::Shaders::Stage::Vertex, "builtin/colored-2d.vert", vertex_source},
             {CE::Assets::Shaders::Stage::Fragment, "builtin/" + name + ".frag", textured ? textured_source : solid_source}}
        };
        CE::Assets::Shaders::MaterialRecipe recipe;
        recipe.id = "cheryl.opengl:" + name + "-material";
        recipe.program = program_recipe.id;
        recipe.vertex_layout = CE::Rendering::Geometry::Layout2D::Position3UV2Color4;
        recipe.topology = CE::Rendering::Geometry::Topology::Triangles;
        recipe.state = {blend, CE::Rendering::Pipelines::Depth::Disabled, false, CE::Rendering::Pipelines::Cull::None};
        recipe.parameters = {{"projection", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::Projection}};
        if (textured)
            recipe.parameters.push_back({"image", CE::Rendering::Parameters::Type::Sampler2D});
        recipe.bindings.emplace(
            "opengl", textured ? R"JSON({"parameters":{"projection":"projectionMatrix","image":"mytexture"}})JSON"
                               : R"JSON({"parameters":{"projection":"projectionMatrix"}})JSON"
        );
        validate_material(recipe, program_recipe);
        auto& cached_program = builtin_programs_[kind];
        auto program = cached_program.lock();
        if (!program) {
            program = build_program(program_recipe);
            cached_program = program;
        }
        auto material = build_material(recipe, program_recipe, program);
        builtin_materials_.insert_or_assign(key, material);
        return material;
    }
}
