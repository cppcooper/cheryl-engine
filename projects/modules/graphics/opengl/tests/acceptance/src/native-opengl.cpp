#include "backends/opengl/gl-test.h"
#if CHERYL_NATIVE_INPUT

#include <gtest/gtest.h>
#include <backends/opengl/glfw-backend.h>
#include <backends/opengl/glfw-context.h>
#include <backends/opengl/renderer.h>
#include <backends/opengl/pipeline.h>
#include <assets/resources/resource-provider.h>
#include <assets/resources/shader-asset-builder.h>
#include <assets/submission/draw2d.h>
#include <assets/types/2d/unicode-text.h>
#include <assets/types/2d/graphic.h>
#include <assets/types/2d/ffont.h>
#include <backends/opengl/resource-provider.h>
#include <backends/opengl/texture.h>
#include <core/display/window.h>
#include <core/display/display-system.h>
#include <core/controls/input-system.h>
#include <core/controls/button-codes.h>
#include <core/game-framework/game-runtime.h>
#include <core/game-framework/abstract-game.h>
#include <core/rendering/render-frame.h>
#include <core/resources/asset-management/material-mgr.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <core/subsystems/event-system.h>
#include <ext/matrix_clip_space.hpp>
#include <ext/matrix_transform.hpp>
#include <internals/exceptions.h>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include "backends/native-glfw/test-access.h"
#include <GLFW/glfw3.h>
#if defined(__linux__) && defined(CHERYL_NATIVE_X11_TESTS)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/keysym.h>
#undef None
#endif

#include <cstdlib>
#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <exception>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {
    bool native_checks_requested() {
        const auto* value = std::getenv("CHERYL_NATIVE_GL_TESTS");
        return value && std::string_view(value) == "1";
    }

    CE::Application::GLFWOpenGL::Options small_window() {
        CE::Application::GLFWOpenGL::Options config;
        config.width = 64;
        config.height = 64;
        config.swap_interval = 0;
        config.title = "Cheryl native acceptance";
        return config;
    }

    GLuint bound_texture_id(const CE::Rendering::Resources::Image& image) {
        image.bind(0);
        GLint id = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &id);
        glBindTexture(GL_TEXTURE_2D, 0);
        return static_cast<GLuint>(id);
    }

    constexpr std::string_view vertex_source = R"(#version 330 core
layout(location = 0) in vec3 in_Position;
out vec3 shade;
void main() { gl_Position = vec4(in_Position, 1.0); shade = vec3(1.0); }
)";
    constexpr std::string_view fragment_source = R"(#version 330 core
in vec3 shade;
uniform vec4 u_color;
out vec4 color;
void main() { color = u_color * vec4(shade, 1.0); }
)";
    constexpr std::string_view mismatched_fragment = R"(#version 330 core
in vec4 shade;
uniform vec4 u_color;
out vec4 color;
void main() { color = u_color * shade; }
)";
    constexpr std::string_view reloaded_fragment = R"(#version 330 core
in vec3 shade;
uniform vec4 u_color;
out vec4 color;
void main() { color = u_color.grba * vec4(shade, 1.0); }
)";

    struct NativeShaderFiles {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("cheryl-native-gl-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        const std::filesystem::path vertex = directory / "test.vert";
        const std::filesystem::path fragment = directory / "test.frag";

        NativeShaderFiles() {
            if (!std::filesystem::create_directory(directory))
                throw CE::Exceptions::failed_operation(CE_HERE, "Could not create native shader test directory");
            try {
                write(vertex, vertex_source);
                write(fragment, fragment_source);
            } catch (...) {
                std::error_code ignored;
                std::filesystem::remove_all(directory, ignored);
                throw;
            }
        }
        ~NativeShaderFiles() {
            std::error_code ignored;
            std::filesystem::remove_all(directory, ignored);
        }
        NativeShaderFiles(const NativeShaderFiles&) = delete;
        NativeShaderFiles& operator=(const NativeShaderFiles&) = delete;
        static void write(const std::filesystem::path& path, const std::string_view source) {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            output << source;
            output.close();
            if (!output)
                throw CE::Exceptions::failed_operation(CE_HERE, "Could not write native shader test source");
        }
        [[nodiscard]] std::vector<std::filesystem::path> stages() const { return {vertex, fragment}; }
    };

    CE::Assets::Materials::Manager::Builder
    material_builder(std::vector<std::filesystem::path> stages, const glm::vec4 color, const bool missing_uniform = false) {
        return [stages = std::move(stages), color, missing_uniform](CE::Rendering::Resources::Provider& resources) {
            auto& native = dynamic_cast<CE::Rendering::OpenGL::ResourceProvider&>(resources);
            CE::Rendering::Pipelines::Definition definition;
            definition.program_sources = stages;
            definition.parameters = {{"color", CE::Rendering::Parameters::Type::Vec4}};
            CE::Rendering::OpenGL::PipelineBindings bindings;
            bindings.parameters = {{"color", missing_uniform ? "absent_uniform" : "u_color", std::nullopt}};
            return native.build_material({native.build_pipeline(std::move(definition), bindings), {{"color", color}}});
        };
    }

    std::shared_ptr<CE::Rendering::Geometry::Buffer2D> fullscreen_triangle(CE::Rendering::Resources::Provider& resources) {
        const std::array<CE::Rendering::Geometry::Vertex2D, 3> vertices{
            {{-1.0f, -1.0f, 0.0f, 0.0f, 0.0f}, {3.0f, -1.0f, 0.0f, 0.0f, 0.0f}, {-1.0f, 3.0f, 0.0f, 0.0f, 0.0f}}
        };
        return resources.upload_geometry(vertices, CE::Rendering::Geometry::Topology::Triangles);
    }

    void retain_draw(
        CE::Rendering::Frame& frame,
        const std::shared_ptr<CE::Rendering::Geometry::Buffer2D>& geometry,
        const std::shared_ptr<const CE::Rendering::Resources::Material>& material
    ) {
        CE::Rendering::FrameWriter writer(frame);
        auto pass = writer.begin_pass(glm::mat4{1.0f}, glm::mat4{1.0f});
        pass.add({geometry, material, 0, 3, material->resolve({}, {}, {}, {})});
    }

    std::array<unsigned char, 4> draw_pixel(CE::Rendering::OpenGL::Renderer& renderer, const CE::Rendering::Frame& frame) {
        renderer.set_viewport({64, 64});
        renderer.clear();
        renderer.render(frame);
        std::array<unsigned char, 4> pixel{};
        glReadBuffer(GL_BACK);
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        EXPECT_EQ(glGetError(), GL_NO_ERROR);
        return pixel;
    }

    CE::Assets::Images::Decoded legacy_font_pixels() {
        // Top-to-bottom source rows, with distinct banks and asymmetric glyphs.
        CE::Assets::Images::Decoded image{{128, 128}, std::vector<unsigned char>(128 * 128 * 4, 0)};
        for (std::size_t pixel = 0; pixel < image.rgba.size(); pixel += 4)
            image.rgba[pixel + 3] = 255;
        const auto paint = [&](const std::size_t index, const std::array<unsigned char, 3> color, const bool asymmetric) {
            for (std::size_t y = 0; y < 8; ++y) {
                for (std::size_t x = 0; x < 8; ++x) {
                    const auto offset = (((index / 16) * 8 + y) * 128 + (index % 16) * 8 + x) * 4;
                    for (std::size_t channel = 0; channel < 3; ++channel)
                        image.rgba[offset + channel] = asymmetric && y >= 4 ? color[channel] / 4 : color[channel];
                }
            }
        };
        paint('A' - 32, {255, 0, 0}, true);
        paint('B' - 32, {0, 255, 0}, false);
        paint(128 + 'A' - 32, {0, 0, 255}, true);
        paint(128 + 'B' - 32, {255, 0, 255}, false);
        return image;
    }

    std::shared_ptr<const CE::Rendering::Resources::Material> legacy_font_material(CE::Rendering::Resources::Provider& resources) {
        auto& native = dynamic_cast<CE::Rendering::OpenGL::ResourceProvider&>(resources);
        const auto shaders = std::filesystem::path(CHERYL_SOURCE_DIR) / "assets/graphics/shaders/shader2d";
        CE::Rendering::Pipelines::Definition definition;
        definition.program_sources = {shaders.string() + ".vert", shaders.string() + ".frag"};
        definition.parameters = {
            {"projection", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::Projection},
            {"view", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::View},
            {"model", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::Model},
            {"alpha", CE::Rendering::Parameters::Type::Float, true, CE::Rendering::Parameters::Semantic::Alpha},
            {"scale", CE::Rendering::Parameters::Type::Float, true, CE::Rendering::Parameters::Semantic::Scale},
            {"image", CE::Rendering::Parameters::Type::Sampler2D}
        };
        const CE::Rendering::OpenGL::PipelineBindings bindings{
            {{"projection", "projectionMatrix"},
             {"view", "viewMatrix"},
             {"model", "modelMatrix"},
             {"alpha", "in_Alpha"},
             {"scale", "in_Scale"},
             {"image", "mytexture"}}
        };
        return native.build_material({native.build_pipeline(std::move(definition), bindings), {}});
    }

    void retain_legacy_text(
        CE::Rendering::Frame& frame,
        const CE::Text::Legacy::BitmapFont& font,
        const std::string_view text,
        const CE::Rendering::DrawStyle2D& style,
        const bool alternate = false
    ) {
        const auto projection = glm::ortho(0.0f, 64.0f, 0.0f, 64.0f, -1.0f, 1.0f);
        CE::Rendering::Submission::Context2D context;
        context.pass = {projection, glm::mat4{1.0f}};
        context.image = CE::Rendering::Submission::ImageParameter2D{"image", 3};
        CE::Rendering::FrameWriter writer(frame);
        auto pass = writer.begin_pass(projection, glm::mat4{1.0f});
        pass.add(CE::Rendering::Submission::resolve_text(font, text, style, context, {alternate}));
    }

    struct NativeFramePixels {
        std::array<unsigned char, 64 * 64 * 4> rgba{};
        std::array<unsigned char, 4> at(const std::size_t x, const std::size_t y) const {
            const auto offset = (y * 64 + x) * 4;
            return {rgba[offset], rgba[offset + 1], rgba[offset + 2], rgba[offset + 3]};
        }
    };

    NativeFramePixels draw_pixels(CE::Rendering::OpenGL::Renderer& renderer, const CE::Rendering::Frame& frame) {
        renderer.set_viewport({64, 64});
        renderer.clear();
        renderer.render(frame);
        NativeFramePixels result;
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, result.rgba.data());
        EXPECT_EQ(glGetError(), GL_NO_ERROR);
        return result;
    }

    std::shared_ptr<const CE::Rendering::Resources::Material> text_run_material(
        CE::Rendering::Resources::Provider& resources,
        const NativeShaderFiles& files,
        const std::shared_ptr<CE::Rendering::Resources::Image>& overlay,
        const CE::Rendering::Sampling::Filter filter,
        const CE::Rendering::Pipelines::Blend blend = CE::Rendering::Pipelines::Blend::StraightAlpha
    ) {
        NativeShaderFiles::write(files.vertex, R"(#version 330 core
layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec2 in_Texcoord;
uniform mat4 u_projection, u_view, u_model;
uniform float u_scale;
out vec2 texcoord;
void main() {
    gl_Position = u_projection * u_view * u_model * vec4(in_Position * u_scale, 1.0);
    texcoord = in_Texcoord;
}
)");
        NativeShaderFiles::write(files.fragment, R"(#version 330 core
in vec2 texcoord;
uniform sampler2D u_image, u_overlay;
uniform float u_alpha;
uniform vec4 u_tint;
out vec4 color;
void main() {
    float coverage = texture(u_image, texcoord).a;
    color = vec4(u_tint.rgb * texture(u_overlay, vec2(0.5)).rgb, coverage * u_alpha * u_tint.a);
}
)");
        CE::Rendering::Pipelines::Definition definition;
        definition.program_sources = files.stages();
        definition.state.blend = blend;
        definition.parameters = {
            {"projection", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::Projection},
            {"view", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::View},
            {"model", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::Model},
            {"alpha", CE::Rendering::Parameters::Type::Float, true, CE::Rendering::Parameters::Semantic::Alpha},
            {"scale", CE::Rendering::Parameters::Type::Float, true, CE::Rendering::Parameters::Semantic::Scale},
            {"image", CE::Rendering::Parameters::Type::Sampler2D},
            {"overlay", CE::Rendering::Parameters::Type::Sampler2D},
            {"tint", CE::Rendering::Parameters::Type::Vec4}
        };
        const CE::Rendering::OpenGL::PipelineBindings bindings{
            {{"projection", "u_projection"},
             {"view", "u_view"},
             {"model", "u_model"},
             {"alpha", "u_alpha"},
             {"scale", "u_scale"},
             {"image", "u_image"},
             {"overlay", "u_overlay"},
             {"tint", "u_tint"}}
        };
        CE::Rendering::Sampling::Options sampling;
        sampling.minification = filter;
        sampling.magnification = filter;
        sampling.mipmaps = CE::Rendering::Sampling::MipmapFilter::None;
        sampling.anisotropy = CE::Rendering::Sampling::Anisotropy::Disabled;
        auto& native = dynamic_cast<CE::Rendering::OpenGL::ResourceProvider&>(resources);
        return native.build_material(
            {native.build_pipeline(std::move(definition), bindings),
             {{"overlay", CE::Rendering::Parameters::ImageBinding{overlay, 5}}, {"tint", glm::vec4{1}}},
             {{"image", resources.create_sampler(sampling)}}}
        );
    }

    CE::Text::FontCollection text_run_fonts() {
        CE::Text::FontSelection selection;
        selection.automatic_system_fonts = false;
        return CE::Text::FontCollection::load(selection);
    }

    void retain_text_runs(
        CE::Rendering::FrameWriter& writer,
        const CE::Text::Rendering::Generation& text,
        const CE::Rendering::DrawStyle2D& style,
        const bool runs,
        const std::uint32_t image_unit = 2
    ) {
        const auto projection = glm::ortho(0.0f, 64.0f, 0.0f, 64.0f, -1.0f, 1.0f);
        const auto view = glm::translate(glm::mat4{1}, {-2, 1, 0});
        auto pass = writer.begin_pass(projection, view);
        const CE::Rendering::Submission::Context2D context{
            pass.semantics(), {}, {}, CE::Rendering::Submission::ImageParameter2D{"image", image_unit}
        };
        pass.add(
            runs ? CE::Rendering::Submission::resolve_text_runs(text, style, context)
                 : CE::Rendering::Submission::resolve_text(text, style, context)
        );
    }

    void expect_same_pixels(const NativeFramePixels& first, const NativeFramePixels& second, const bool ink = true) {
        int largest_difference = 0;
        std::size_t colored = 0, differences = 0, greatest_index = 0;
        for (std::size_t index = 0; index < first.rgba.size(); ++index) {
            const auto difference = std::abs(int(first.rgba[index]) - int(second.rgba[index]));
            if (difference > largest_difference) {
                largest_difference = difference;
                greatest_index = index;
            }
            if (difference > 1)
                ++differences;
            if (index % 4 != 3 && first.rgba[index] != 0)
                ++colored;
        }
        if (largest_difference > 1) {
            const auto stem = std::filesystem::temp_directory_path() /
                              ("cheryl-text-run-diff-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            const auto save = [&](const NativeFramePixels& pixels, const std::string_view suffix) {
                const auto path = stem.string() + std::string(suffix) + ".ppm";
                std::ofstream output(path, std::ios::binary);
                output << "P6\n64 64\n255\n";
                for (int row = 63; row >= 0; --row)
                    for (std::size_t column = 0; column < 64; ++column)
                        output.write(reinterpret_cast<const char*>(pixels.rgba.data() + (row * 64 + column) * 4), 3);
                output.close();
                EXPECT_TRUE(output.good()) << path;
                return path;
            };
            ADD_FAILURE() << "Text images: " << save(first, "-glyphs") << " and " << save(second, "-runs");
        }
        EXPECT_LE(largest_difference, 1) << differences << " channels outside tolerance; largest at " << (greatest_index / 4) % 64 << ','
                                         << (greatest_index / 4) / 64 << " channel " << greatest_index % 4 << ": "
                                         << int(first.rgba[greatest_index]) << " vs " << int(second.rgba[greatest_index]);
        if (ink)
            EXPECT_GT(colored, 12u);
    }

    struct NativeDrawTrace {
        inline static NativeDrawTrace* active = nullptr;
        const PFNGLDRAWARRAYSPROC original = glad_glDrawArrays;
        std::size_t draws = 0;

        NativeDrawTrace() {
            active = this;
            glad_glDrawArrays = record;
        }
        ~NativeDrawTrace() {
            glad_glDrawArrays = original;
            active = nullptr;
        }
        NativeDrawTrace(const NativeDrawTrace&) = delete;
        NativeDrawTrace& operator=(const NativeDrawTrace&) = delete;
        static void GLAD_API_PTR record(const GLenum mode, const GLint first, const GLsizei count) {
            ++active->draws;
            active->original(mode, first, count);
        }
    };

    // Observe real driver-created IDs without allocating inside the forwarding calls.
    // All other GL calls remain native, and the entry points are restored on every exit.
    struct NativeCreationTrace {
        inline static NativeCreationTrace* active = nullptr;
        const PFNGLCREATEPROGRAMPROC original_program = glad_glCreateProgram;
        const PFNGLCREATESHADERPROC original_shader = glad_glCreateShader;
        std::array<GLuint, 4> programs{};
        std::array<GLuint, 4> shaders{};
        std::size_t program_count = 0;
        std::size_t shader_count = 0;
        bool overflow = false;

        NativeCreationTrace() {
            if (active)
                throw CE::Exceptions::failed_operation(CE_HERE, "Native creation tracing cannot nest");
            active = this;
            glad_glCreateProgram = create_program;
            glad_glCreateShader = create_shader;
        }
        ~NativeCreationTrace() {
            glad_glCreateProgram = original_program;
            glad_glCreateShader = original_shader;
            active = nullptr;
        }
        NativeCreationTrace(const NativeCreationTrace&) = delete;
        NativeCreationTrace& operator=(const NativeCreationTrace&) = delete;
        static GLuint GLAD_API_PTR create_program() {
            const auto id = active->original_program();
            if (active->program_count < active->programs.size())
                active->programs[active->program_count++] = id;
            else
                active->overflow = true;
            return id;
        }
        static GLuint GLAD_API_PTR create_shader(const GLenum kind) {
            const auto id = active->original_shader(kind);
            if (active->shader_count < active->shaders.size())
                active->shaders[active->shader_count++] = id;
            else
                active->overflow = true;
            return id;
        }
        void reset() {
            program_count = shader_count = 0;
            overflow = false;
        }
        void expect_deleted(const std::size_t expected_stages) const {
            EXPECT_FALSE(overflow);
            EXPECT_EQ(program_count, 1u);
            EXPECT_EQ(shader_count, expected_stages);
            for (std::size_t i = 0; i < program_count; ++i) {
                EXPECT_NE(programs[i], 0u);
                EXPECT_EQ(glIsProgram(programs[i]), GL_FALSE);
            }
            for (std::size_t i = 0; i < shader_count; ++i) {
                EXPECT_NE(shaders[i], 0u);
                EXPECT_EQ(glIsShader(shaders[i]), GL_FALSE);
            }
            EXPECT_EQ(glGetError(), GL_NO_ERROR);
        }
    };
}

TEST(native_opengl, builtin_materials) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& renderer = dynamic_cast<CE::Rendering::OpenGL::Renderer&>(engine->renderer());
    auto& resources = engine->resources();
    renderer.initialize();
    auto* builder = resources.shader_asset_builder();
    ASSERT_NE(builder, nullptr);
    using Kind = CE::Rendering::Materials::Builtin2D;
    using Blend = CE::Rendering::Pipelines::Blend;
    std::vector<std::shared_ptr<const CE::Rendering::Resources::Material>> retained;
    std::array<GLuint, 2> programs{};
    renderer.set_viewport({64, 64});
    renderer.set_clear_colour(0, 0, 0, 1);
    for (const auto blend : {Blend::StraightAlpha, Blend::PremultipliedAlpha}) {
        for (const auto kind : {Kind::SolidColor, Kind::TexturedColor}) {
            const auto material = builder->build_builtin_material(kind, blend);
            ASSERT_TRUE(material);
            EXPECT_EQ(builder->build_builtin_material(kind, blend), material);
            EXPECT_EQ(material->definition().pipeline->definition().state.blend, blend);
            retained.push_back(material);
            const bool textured = kind == Kind::TexturedColor;
            const bool premultiplied = blend == Blend::PremultipliedAlpha;
            const glm::vec4 color = textured ? (premultiplied ? glm::vec4{0.25f, 0.5f, 0.125f, 0.5f} : glm::vec4{0.5f, 1, 0.25f, 0.5f})
                                             : glm::vec4{premultiplied ? 0.5f : 1.0f, 0, 0, 0.5f};
            const std::array<CE::Rendering::Geometry::Vertex2DColor, 3> vertices{
                {{0, 0, 0, 0, 0, color.r, color.g, color.b, color.a},
                 {128, 0, 0, 0, 0, color.r, color.g, color.b, color.a},
                 {0, 128, 0, 0, 0, color.r, color.g, color.b, color.a}}
            };
            const auto geometry = resources.upload_geometry(vertices, CE::Rendering::Geometry::Topology::Triangles);
            CE::Rendering::DrawStyle2D style;
            style.material = material;
            if (textured) {
                CE::Assets::Images::Decoded image{
                    {1, 1}, premultiplied ? std::vector<unsigned char>{64, 32, 128, 128} : std::vector<unsigned char>{128, 64, 255, 128}
                };
                style.parameters.emplace("image", CE::Rendering::Parameters::ImageBinding{resources.create_image(image), 0});
            }
            CE::Rendering::Frame frame;
            {
                CE::Rendering::FrameWriter writer(frame);
                auto pass = writer.begin_pass(glm::ortho(0.0f, 64.0f, 64.0f, 0.0f), glm::mat4{1});
                pass.add(CE::Rendering::resolve_draw_packet(geometry, 0, 3, style, pass.semantics(), {}, pass.constraints()));
            }
            renderer.clear();
            renderer.render(frame);
            std::array<unsigned char, 4> pixel{};
            glReadBuffer(GL_BACK);
            glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            EXPECT_NEAR(pixel[0], textured ? 16 : 128, 1);
            EXPECT_NEAR(pixel[1], textured ? 16 : 0, 1);
            EXPECT_NEAR(pixel[2], textured ? 16 : 0, 1);
            EXPECT_EQ(pixel[3], 255);
            GLint program = 0;
            glGetIntegerv(GL_CURRENT_PROGRAM, &program);
            if (blend == Blend::StraightAlpha)
                programs[textured ? 1 : 0] = static_cast<GLuint>(program);
            else
                EXPECT_EQ(static_cast<GLuint>(program), programs[textured ? 1 : 0]);
            EXPECT_EQ(glGetError(), GL_NO_ERROR);
        }
    }
    EXPECT_NE(programs[0], programs[1]);
    EXPECT_THROW(
        static_cast<void>(builder->build_builtin_material(static_cast<Kind>(99), Blend::StraightAlpha)), CE::Exceptions::invalid_args
    );
    EXPECT_THROW(
        static_cast<void>(builder->build_builtin_material(Kind::SolidColor, static_cast<Blend>(99))), CE::Exceptions::invalid_args
    );
    std::exception_ptr foreign_error;
    std::thread foreign([&] {
        try {
            static_cast<void>(builder->build_builtin_material(Kind::SolidColor, Blend::StraightAlpha));
        } catch (...) {
            foreign_error = std::current_exception();
        }
    });
    foreign.join();
    ASSERT_TRUE(foreign_error);
    EXPECT_THROW(std::rethrow_exception(foreign_error), CE::Exceptions::failed_operation);
    {
        CE::Rendering::OpenGL::ResourceProvider other(renderer);
        EXPECT_NE(other.shader_asset_builder()->build_builtin_material(Kind::SolidColor, Blend::StraightAlpha), retained.front());
    }
    const std::weak_ptr<const CE::Rendering::Resources::Material> lifetime = retained.front();
    retained.clear();
    EXPECT_TRUE(lifetime.expired());
    glUseProgram(0);
    renderer.maintain_resources();
    EXPECT_EQ(glIsProgram(programs[0]), GL_FALSE);
    EXPECT_EQ(glIsProgram(programs[1]), GL_FALSE);
    const auto warm = builder->build_builtin_material(Kind::SolidColor, Blend::StraightAlpha);
    ASSERT_TRUE(warm);
    renderer.deinitialize();
    EXPECT_THROW(
        static_cast<void>(builder->build_builtin_material(Kind::SolidColor, Blend::StraightAlpha)), CE::Exceptions::failed_operation
    );
}

TEST(native_opengl, asset_materials) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& renderer = dynamic_cast<CE::Rendering::OpenGL::Renderer&>(engine->renderer());
    auto& resources = engine->resources();
    renderer.initialize();
    renderer.set_clear_colour(0, 0, 0, 1);
    using Kind = CE::Rendering::Materials::Builtin2D;
    using Blend = CE::Rendering::Pipelines::Blend;
    std::vector<std::shared_ptr<const CE::Rendering::Resources::Material>> retained;
    std::array<GLuint, 2> programs{};
    for (const auto blend : {Blend::StraightAlpha, Blend::PremultipliedAlpha}) {
        const bool premultiplied = blend == Blend::PremultipliedAlpha;
        for (const auto kind : {Kind::Textured, Kind::TexturedStrip}) {
            const auto material = CE::Assets::Shaders::build_builtin_material(resources, kind, blend);
            ASSERT_TRUE(material);
            EXPECT_EQ(CE::Assets::Shaders::build_builtin_material(resources, kind, blend), material);
            EXPECT_TRUE(material->definition().sampling.empty());
            retained.push_back(material);
            const auto image = resources.create_image(
                {{1, 1}, premultiplied ? std::vector<unsigned char>{64, 32, 128, 128} : std::vector<unsigned char>{128, 64, 255, 128}}
            );
            CE::Rendering::DrawStyle2D style;
            style.scale = 16;
            style.alpha = 0.5f;
            style.model_matrix = glm::translate(glm::mat4{1}, glm::vec3{8, 52, 0});
            CE::Rendering::Frame frame;
            {
                CE::Rendering::FrameWriter writer(frame);
                auto pass = writer.begin_pass(glm::ortho(0.0f, 64.0f, 0.0f, 64.0f), glm::translate(glm::mat4{1}, glm::vec3{16, -12, 0}));
                const CE::Rendering::Submission::Context2D context{pass.semantics(), pass.parameters(), pass.constraints()};
                if (kind == Kind::Textured) {
                    const auto graphic = CE::Assets::Graphics::Asset::from_image(image, resources, {0, 0}, material);
                    pass.add(CE::Rendering::Submission::resolve_graphic(graphic, style, context));
                } else {
                    const std::array<CE::Rendering::Geometry::Vertex2D, 4> vertices{
                        {{0, -1, 0, 0, 0}, {1, -1, 0, 1, 0}, {0, 0, 0, 0, 1}, {1, 0, 0, 1, 1}}
                    };
                    auto geometry = resources.upload_geometry(vertices, CE::Rendering::Geometry::Topology::TriangleStrip);
                    CE::Assets::Sprites::Definition definition;
                    definition.grid.frame = {1, 1};
                    definition.grid.rows = definition.grid.columns = 1;
                    const CE::Assets::Sprites::Asset sprite({std::move(geometry), image, definition, material});
                    pass.add(CE::Rendering::Submission::resolve_sprite(sprite, 0, style, context));
                }
            }
            const auto pixel = draw_pixel(renderer, frame);
            EXPECT_NEAR(pixel[0], 32, 1);
            EXPECT_NEAR(pixel[1], 16, 1);
            EXPECT_NEAR(pixel[2], 64, 1);
            EXPECT_EQ(pixel[3], 255);
            std::array<unsigned char, 4> outside{};
            glReadPixels(48, 48, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, outside.data());
            EXPECT_EQ(outside, (std::array<unsigned char, 4>{0, 0, 0, 255}));
            GLint program = 0;
            glGetIntegerv(GL_CURRENT_PROGRAM, &program);
            if (kind == Kind::Textured)
                programs[premultiplied ? 1 : 0] = static_cast<GLuint>(program);
            else
                EXPECT_EQ(static_cast<GLuint>(program), programs[premultiplied ? 1 : 0]);
            EXPECT_EQ(glGetError(), GL_NO_ERROR);
        }
    }
    EXPECT_NE(programs[0], programs[1]);
    {
        const std::array<unsigned char, 1> coverage{128};
        auto atlas = resources.create_font_atlas(coverage, {1, 1});
        const auto graphic = CE::Assets::Graphics::Asset::from_image(atlas, resources, {0, 0});
        CE::Rendering::DrawStyle2D style;
        style.scale = 64;
        style.model_matrix = glm::translate(glm::mat4{1}, glm::vec3{0, 64, 0});
        CE::Rendering::Frame frame;
        CE::Rendering::FrameWriter writer(frame);
        auto pass = writer.begin_pass(glm::ortho(0.0f, 64.0f, 0.0f, 64.0f), glm::mat4{1});
        pass.add(CE::Rendering::Submission::resolve_graphic(graphic, style, {pass.semantics(), pass.parameters(), pass.constraints()}));
        const auto pixel = draw_pixel(renderer, frame);
        for (std::size_t channel = 0; channel < 3; ++channel)
            EXPECT_NEAR(pixel[channel], 128, 1);
        EXPECT_EQ(pixel[3], 255);
    }
    renderer.deinitialize();
}

TEST(native_opengl, ui_clipping_color) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& renderer = dynamic_cast<CE::Rendering::OpenGL::Renderer&>(engine->renderer());
    auto& resources = dynamic_cast<CE::Rendering::OpenGL::ResourceProvider&>(engine->resources());
    renderer.initialize();
    NativeShaderFiles files;
    NativeShaderFiles::write(files.vertex, R"(#version 330 core
layout(location = 0) in vec3 in_Position;
layout(location = 2) in vec4 in_Color;
out vec4 tint;
void main() { gl_Position = vec4(in_Position, 1.0); tint = in_Color; }
)");
    NativeShaderFiles::write(files.fragment, R"(#version 330 core
in vec4 tint;
out vec4 color;
void main() { color = tint; }
)");
    CE::Rendering::Pipelines::Definition definition;
    definition.program_sources = files.stages();
    definition.vertex_layout = CE::Rendering::Geometry::Layout2D::Position3UV2Color4;
    auto material = resources.build_material({resources.build_pipeline(definition, {}), {}});
    const auto upload = [&](const glm::vec4 color) {
        const std::array<CE::Rendering::Geometry::Vertex2DColor, 3> vertices{
            {{-1, -1, 0, 0, 0, color.r, color.g, color.b, color.a},
             {3, -1, 0, 0, 0, color.r, color.g, color.b, color.a},
             {-1, 3, 0, 0, 0, color.r, color.g, color.b, color.a}}
        };
        return resources.upload_geometry(vertices, CE::Rendering::Geometry::Topology::Triangles);
    };
    const auto red = upload({1, 0, 0, 0.5f});
    const auto blue = upload({0, 0, 1, 0.5f});
    const auto green = upload({0, 1, 0, 1});
    CE::Rendering::Frame frame;
    {
        CE::Rendering::FrameWriter writer(frame);
        auto pass = writer.begin_pass(glm::mat4{1}, glm::mat4{1});
        CE::Rendering::DrawStyle2D style;
        style.material = material;
        pass.add(CE::Rendering::resolve_draw_packet(red, 0, 3, style, {}, {}, {}));
        style.clip = CE::Rendering::ClipRegion2D{{0, 0, 16, 16}, 32, 32};
        pass.add(CE::Rendering::resolve_draw_packet(blue, 0, 3, style, {}, {}, {}));
        style.clip->rectangle.right = 0; // Empty clips must not expose the green triangle.
        pass.add(CE::Rendering::resolve_draw_packet(green, 0, 3, style, {}, {}, {}));
    }
    const auto pixel = [&](int x, int y) {
        std::array<unsigned char, 4> result{};
        glReadBuffer(GL_BACK);
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
        EXPECT_EQ(glGetError(), GL_NO_ERROR);
        return result;
    };
    renderer.set_viewport({64, 64});
    renderer.set_clear_colour(0, 0, 0, 1);
    renderer.clear();
    renderer.render(frame);
    const auto overlap = pixel(16, 48); // Logical top left maps to OpenGL's upper rows.
    EXPECT_NEAR(overlap[0], 64, 1);
    EXPECT_EQ(overlap[1], 0);
    EXPECT_NEAR(overlap[2], 128, 1);
    const auto outside = pixel(16, 16);
    EXPECT_NEAR(outside[0], 128, 1);
    EXPECT_EQ(outside[1], 0);
    EXPECT_EQ(outside[2], 0);
    CE::Rendering::Frame unclipped;
    {
        CE::Rendering::FrameWriter writer(unclipped);
        auto pass = writer.begin_pass(glm::mat4{1}, glm::mat4{1});
        CE::Rendering::DrawStyle2D style;
        style.material = material;
        pass.add(CE::Rendering::resolve_draw_packet(green, 0, 3, style, {}, {}, {}));
    }
    renderer.render(unclipped);
    EXPECT_FALSE(glIsEnabled(GL_SCISSOR_TEST));
    EXPECT_EQ(pixel(48, 16), (std::array<unsigned char, 4>{0, 255, 0, 255}));
    renderer.render(frame); // Leaves the last nonempty draw clipped.
    renderer.clear();
    EXPECT_FALSE(glIsEnabled(GL_SCISSOR_TEST));
    EXPECT_EQ(pixel(48, 16), (std::array<unsigned char, 4>{0, 0, 0, 255}));
    frame.recycle();
    unclipped.recycle();
    renderer.deinitialize();
}

TEST(native_opengl, deferred_worker_release) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& renderer = dynamic_cast<CE::Rendering::OpenGL::Renderer&>(engine->renderer());
    renderer.initialize();
    auto image = engine->resources().create_image(CE::Assets::Images::Decoded{{1, 1}, {255, 255, 255, 255}});
    const auto id = bound_texture_id(*image);
    ASSERT_NE(id, 0u);
    ASSERT_EQ(glIsTexture(id), GL_TRUE);

    // The final shared owner leaves on a worker; that thread must only retire it.
    std::thread release([image = std::move(image)]() mutable { image.reset(); });
    release.join();
    EXPECT_EQ(glIsTexture(id), GL_TRUE);
    renderer.maintain_resources();
    EXPECT_EQ(glIsTexture(id), GL_FALSE);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    renderer.deinitialize();
}

TEST(native_opengl, texture_sampling) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& renderer = dynamic_cast<CE::Rendering::OpenGL::Renderer&>(engine->renderer());
    auto& resources = dynamic_cast<CE::Rendering::OpenGL::ResourceProvider&>(engine->resources());
    renderer.initialize();
    NativeShaderFiles files;
    NativeShaderFiles::write(files.vertex, R"(#version 330 core
layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec2 in_Texcoord;
out vec2 uv;
void main() { gl_Position = vec4(in_Position, 1.0); uv = in_Texcoord; }
)");
    NativeShaderFiles::write(files.fragment, R"(#version 330 core
in vec2 uv;
uniform sampler2D pixels;
out vec4 color;
void main() { color = texture(pixels, uv); }
)");
    CE::Rendering::Pipelines::Definition definition;
    definition.program_sources = files.stages();
    definition.parameters = {{"image", CE::Rendering::Parameters::Type::Sampler2D}};
    const auto pipeline = resources.build_pipeline(definition, {{{"image", "pixels"}}});
    const auto image = resources.create_image(CE::Assets::Images::Decoded{{2, 1}, {255, 0, 0, 255, 0, 0, 255, 255}});
    const auto geometry = [&](const float scale) {
        // The center readback samples u=0.625 for both derivatives. Scale 1
        // magnifies this two-texel image; scale 128 minifies it without mip access.
        const auto left = 0.625f - scale * (32.5f / 64);
        const std::array<CE::Rendering::Geometry::Vertex2D, 3> vertices{
            {{-1, -1, 0, left, 0.5f}, {3, -1, 0, left + 2 * scale, 0.5f}, {-1, 3, 0, left, 0.5f}}
        };
        return resources.upload_geometry(vertices, CE::Rendering::Geometry::Topology::Triangles);
    };
    const std::array geometries{geometry(1), geometry(128)};
    for (const auto minification : {CE::Rendering::Sampling::Filter::Nearest, CE::Rendering::Sampling::Filter::Linear}) {
        for (const auto magnification : {CE::Rendering::Sampling::Filter::Nearest, CE::Rendering::Sampling::Filter::Linear}) {
            SCOPED_TRACE(static_cast<int>(minification));
            SCOPED_TRACE(static_cast<int>(magnification));
            CE::Rendering::Sampling::Options options;
            options.minification = minification;
            options.magnification = magnification;
            options.mipmaps = CE::Rendering::Sampling::MipmapFilter::None;
            options.anisotropy = CE::Rendering::Sampling::Anisotropy::Disabled;
            const auto sampler = resources.create_sampler(options);
            const auto material =
                resources.build_material({pipeline, {{"image", CE::Rendering::Parameters::ImageBinding{image, 0, sampler}}}});
            std::array<CE::Rendering::Frame, 2> frames;
            for (std::size_t i = 0; i < frames.size(); ++i) {
                retain_draw(frames[i], geometries[i], material);
                const auto pixel = draw_pixel(renderer, frames[i]);
                const auto filter = i == 0 ? magnification : minification;
                if (filter == CE::Rendering::Sampling::Filter::Nearest) {
                    EXPECT_EQ(pixel, (std::array<unsigned char, 4>{0, 0, 255, 255}));
                } else {
                    EXPECT_NEAR(pixel[0], 64, 2);
                    EXPECT_EQ(pixel[1], 0);
                    EXPECT_NEAR(pixel[2], 191, 2);
                    EXPECT_EQ(pixel[3], 255);
                }
            }
            CE::Rendering::Sampling::Options different = options;
            different.magnification = magnification == CE::Rendering::Sampling::Filter::Nearest ? CE::Rendering::Sampling::Filter::Linear
                                                                                                : CE::Rendering::Sampling::Filter::Nearest;
            const auto alternate = resources.create_sampler(different);
            alternate->bind(0);
            const auto retained = draw_pixel(renderer, frames[0]);
            if (magnification == CE::Rendering::Sampling::Filter::Nearest)
                EXPECT_EQ(retained, (std::array<unsigned char, 4>{0, 0, 255, 255}));
            else
                EXPECT_NEAR(retained[0], 64, 2);
        }
    }
    const auto default_sampler = resources.create_sampler({});
    default_sampler->bind(0);
    GLint sampler_id = 0;
    glGetIntegeri_v(GL_SAMPLER_BINDING, 0, &sampler_id);
    ASSERT_NE(sampler_id, 0);
    if (GLAD_GL_VERSION_4_6 || GLAD_GL_ARB_texture_filter_anisotropic || GLAD_GL_EXT_texture_filter_anisotropic) {
        GLfloat maximum = 0, effective = 0;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maximum);
        glGetSamplerParameterfv(static_cast<GLuint>(sampler_id), GL_TEXTURE_MAX_ANISOTROPY, &effective);
        EXPECT_FLOAT_EQ(effective, maximum);
        EXPECT_FLOAT_EQ(default_sampler->effective_anisotropy(), maximum);
    } else {
        EXPECT_FLOAT_EQ(default_sampler->effective_anisotropy(), 1);
    }
    CE::Rendering::Frame defaults;
    const auto default_material = resources.build_material({pipeline, {{"image", CE::Rendering::Parameters::ImageBinding{image, 0}}}});
    retain_draw(defaults, geometries[0], default_material);
    static_cast<void>(draw_pixel(renderer, defaults));
    glGetIntegeri_v(GL_SAMPLER_BINDING, 0, &sampler_id);
    EXPECT_EQ(sampler_id, 0);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    defaults.recycle();
    renderer.deinitialize();
}

TEST(native_opengl, debug_output) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& renderer = dynamic_cast<CE::Rendering::OpenGL::Renderer&>(engine->renderer());
    auto& context = dynamic_cast<CE::Rendering::OpenGL::Context&>(engine->surface());
    void* previous_callback = nullptr;
    void* previous_user = nullptr;
    GLboolean previous_enabled = GL_FALSE;
    GLboolean previous_synchronous = GL_FALSE;
    bool supported = false;
    {
        CE::Rendering::OpenGL::Renderer probe(context);
        probe.initialize();
        supported = GLAD_GL_VERSION_4_3 || GLAD_GL_KHR_debug;
        if (supported) {
            glGetPointerv(GL_DEBUG_CALLBACK_FUNCTION, &previous_callback);
            glGetPointerv(GL_DEBUG_CALLBACK_USER_PARAM, &previous_user);
            previous_enabled = glIsEnabled(GL_DEBUG_OUTPUT);
            previous_synchronous = glIsEnabled(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        }
        EXPECT_EQ(glGetError(), GL_NO_ERROR);
        probe.deinitialize();
    }
    renderer.set_native_diagnostics(true);
    renderer.initialize();
    if (supported) {
        void* callback = nullptr;
        void* user = nullptr;
        glGetPointerv(GL_DEBUG_CALLBACK_FUNCTION, &callback);
        glGetPointerv(GL_DEBUG_CALLBACK_USER_PARAM, &user);
        EXPECT_NE(callback, nullptr);
        if (previous_callback) {
            EXPECT_EQ(callback, previous_callback);
            EXPECT_EQ(user, previous_user);
            EXPECT_EQ(glIsEnabled(GL_DEBUG_OUTPUT), previous_enabled);
            EXPECT_EQ(glIsEnabled(GL_DEBUG_OUTPUT_SYNCHRONOUS), previous_synchronous);
        } else {
            EXPECT_EQ(user, nullptr);
            EXPECT_EQ(glIsEnabled(GL_DEBUG_OUTPUT_SYNCHRONOUS), GL_TRUE);
        }
    }
    renderer.clear();
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    renderer.deinitialize();
    context.make_current();
    if (supported) {
        void* callback = nullptr;
        void* user = nullptr;
        glGetPointerv(GL_DEBUG_CALLBACK_FUNCTION, &callback);
        glGetPointerv(GL_DEBUG_CALLBACK_USER_PARAM, &user);
        EXPECT_EQ(callback, previous_callback);
        EXPECT_EQ(user, previous_user);
        EXPECT_EQ(glIsEnabled(GL_DEBUG_OUTPUT), previous_enabled);
        EXPECT_EQ(glIsEnabled(GL_DEBUG_OUTPUT_SYNCHRONOUS), previous_synchronous);
    }
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    context.release_current();
}

TEST(native_opengl, shutdown_context_restoration) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& renderer = dynamic_cast<CE::Rendering::OpenGL::Renderer&>(engine->renderer());
    renderer.initialize();
    auto image = engine->resources().create_image(CE::Assets::Images::Decoded{{1, 1}, {255, 255, 255, 255}});
    const auto id = bound_texture_id(*image);
    auto* selected = dynamic_cast<CE::Platform::GLFW::Window&>(engine->window()).native_handle();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> other(
        glfwCreateWindow(32, 32, "Other native context", nullptr, nullptr), glfwDestroyWindow
    );
    ASSERT_NE(other, nullptr);
    glfwMakeContextCurrent(other.get());
    ASSERT_EQ(glfwGetCurrentContext(), other.get());

    EXPECT_THROW(image->bind(0), CE::Exceptions::failed_operation);
    EXPECT_THROW(renderer.maintain_resources(), CE::Exceptions::failed_operation);
    // Shutdown must select its borrowed window's context before deleting its handles.
    renderer.deinitialize();
    EXPECT_EQ(glfwGetCurrentContext(), nullptr);
    glfwMakeContextCurrent(selected);
    EXPECT_EQ(glIsTexture(id), GL_FALSE);
    EXPECT_THROW(image->bind(0), CE::Exceptions::failed_operation);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    glfwMakeContextCurrent(nullptr);
    other.reset();
    engine.reset();

    // A retained image outlives both GLFW contexts and then leaves on another thread.
    std::thread release([image = std::move(image)]() mutable { image.reset(); });
    release.join();
}

TEST(native_opengl, retained_frame_reload) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& renderer = dynamic_cast<CE::Rendering::OpenGL::Renderer&>(engine->renderer());
    renderer.initialize();
    NativeShaderFiles files;
    auto& materials = CE::Assets::Materials::Manager::get();
    const auto key = files.directory / "material";
    materials.load(key, engine->resources(), material_builder(files.stages(), {1.0f, 0.0f, 0.0f, 1.0f}));
    auto old = materials.get_asset(key);
    auto geometry = fullscreen_triangle(engine->resources());
    std::weak_ptr<const CE::Rendering::Resources::Material> old_owner = old;
    std::weak_ptr<CE::Rendering::Geometry::Buffer2D> geometry_owner = geometry;
    GLint vao = 0, buffer = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    ASSERT_NE(vao, 0);
    ASSERT_NE(buffer, 0);
    CE::Rendering::Frame old_frame, new_frame;
    retain_draw(old_frame, geometry, old);
    EXPECT_EQ(draw_pixel(renderer, old_frame), (std::array<unsigned char, 4>{255, 0, 0, 255}));
    GLint old_program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);

    // Keep the authored color red; only the new shader swaps red and green.
    NativeShaderFiles::write(files.fragment, reloaded_fragment);
    materials.reload(key, engine->resources(), material_builder(files.stages(), {1.0f, 0.0f, 0.0f, 1.0f}));
    auto current = materials.get_asset(key);
    ASSERT_NE(old, current);
    retain_draw(new_frame, geometry, current);
    materials.clear_assets();
    old.reset();
    current.reset();
    geometry.reset();
    EXPECT_FALSE(materials.contains(key));
    EXPECT_EQ(draw_pixel(renderer, old_frame), (std::array<unsigned char, 4>{255, 0, 0, 255}));
    EXPECT_EQ(draw_pixel(renderer, new_frame), (std::array<unsigned char, 4>{0, 255, 0, 255}));
    GLint new_program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &new_program);
    EXPECT_NE(old_program, new_program);
    old_frame.recycle();
    EXPECT_TRUE(old_owner.expired());
    EXPECT_EQ(glIsProgram(old_program), GL_TRUE);
    renderer.maintain_resources();
    EXPECT_EQ(glIsProgram(old_program), GL_FALSE);
    EXPECT_EQ(glIsProgram(new_program), GL_TRUE);
    EXPECT_FALSE(geometry_owner.expired());

    EXPECT_EQ(draw_pixel(renderer, new_frame), (std::array<unsigned char, 4>{0, 255, 0, 255}));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    new_frame.recycle();
    EXPECT_TRUE(geometry_owner.expired());
    EXPECT_EQ(glIsProgram(new_program), GL_TRUE);
    EXPECT_EQ(glIsVertexArray(vao), GL_TRUE);
    EXPECT_EQ(glIsBuffer(buffer), GL_TRUE);
    renderer.maintain_resources();
    EXPECT_EQ(glIsProgram(new_program), GL_FALSE);
    EXPECT_EQ(glIsVertexArray(vao), GL_FALSE);
    EXPECT_EQ(glIsBuffer(buffer), GL_FALSE);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    renderer.deinitialize();
}

TEST(native_opengl, failed_reload_cleanup) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& renderer = dynamic_cast<CE::Rendering::OpenGL::Renderer&>(engine->renderer());
    renderer.initialize();
    NativeShaderFiles files;
    auto& materials = CE::Assets::Materials::Manager::get();
    const auto key = files.directory / "material";
    const auto recipe = material_builder(files.stages(), {1.0f, 0.0f, 0.0f, 1.0f});
    materials.load(key, engine->resources(), recipe);
    const auto published = materials.get_asset(key);
    auto geometry = fullscreen_triangle(engine->resources());
    CE::Rendering::Frame frame;
    retain_draw(frame, geometry, published);
    NativeCreationTrace trace;

    NativeShaderFiles::write(files.fragment, "#version 330 core\nthis is not valid GLSL;\n");
    EXPECT_THROW(materials.reload(key, engine->resources(), recipe), CE::Exceptions::runtime_exception);
    trace.expect_deleted(2);
    EXPECT_EQ(materials.get_asset(key), published);
    EXPECT_EQ(draw_pixel(renderer, frame), (std::array<unsigned char, 4>{255, 0, 0, 255}));

    trace.reset();
    NativeShaderFiles::write(files.fragment, mismatched_fragment);
    EXPECT_THROW(materials.reload(key, engine->resources(), recipe), CE::Exceptions::runtime_exception);
    trace.expect_deleted(2);
    EXPECT_EQ(materials.get_asset(key), published);
    EXPECT_EQ(draw_pixel(renderer, frame), (std::array<unsigned char, 4>{255, 0, 0, 255}));

    trace.reset();
    std::filesystem::remove(files.fragment);
    EXPECT_THROW(materials.reload(key, engine->resources(), recipe), CE::Exceptions::runtime_exception);
    trace.expect_deleted(1);
    EXPECT_EQ(materials.get_asset(key), published);
    EXPECT_EQ(draw_pixel(renderer, frame), (std::array<unsigned char, 4>{255, 0, 0, 255}));

    trace.reset();
    NativeShaderFiles::write(files.fragment, fragment_source);
    EXPECT_THROW(
        materials.reload(key, engine->resources(), material_builder(files.stages(), {0.0f, 1.0f, 0.0f, 1.0f}, true)),
        CE::Exceptions::invalid_args
    );
    ASSERT_EQ(trace.program_count, 1u);
    EXPECT_EQ(glIsProgram(trace.programs[0]), GL_TRUE); // Linked candidate retired after reflection rejection.
    renderer.maintain_resources();
    trace.expect_deleted(2);
    EXPECT_EQ(materials.get_asset(key), published);
    EXPECT_EQ(draw_pixel(renderer, frame), (std::array<unsigned char, 4>{255, 0, 0, 255}));

    trace.reset();
    materials.reload(key, engine->resources(), recipe);
    EXPECT_NE(materials.get_asset(key), published);
    EXPECT_EQ(draw_pixel(renderer, frame), (std::array<unsigned char, 4>{255, 0, 0, 255}));
    frame.recycle();
    renderer.deinitialize();
}

TEST(native_opengl, shutdown_before_late_release) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& renderer = dynamic_cast<CE::Rendering::OpenGL::Renderer&>(engine->renderer());
    renderer.initialize();
    NativeShaderFiles files;
    auto material = material_builder(files.stages(), {1.0f, 0.0f, 0.0f, 1.0f})(engine->resources());
    auto geometry = fullscreen_triangle(engine->resources());
    auto image = engine->resources().create_image(CE::Assets::Images::Decoded{{1, 1}, {255, 255, 255, 255}});
    auto sampler = engine->resources().create_sampler({});
    sampler->bind(2);
    GLint sampler_id = 0;
    glGetIntegeri_v(GL_SAMPLER_BINDING, 2, &sampler_id);
    ASSERT_NE(sampler_id, 0);
    const auto texture = bound_texture_id(*image);
    CE::Rendering::Frame frame;
    retain_draw(frame, geometry, material);
    EXPECT_EQ(draw_pixel(renderer, frame), (std::array<unsigned char, 4>{255, 0, 0, 255}));
    GLint program = 0, vao = 0, buffer = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    ASSERT_NE(program, 0);
    ASSERT_NE(vao, 0);
    ASSERT_NE(buffer, 0);
    auto* selected = dynamic_cast<CE::Platform::GLFW::Window&>(engine->window()).native_handle();
    const auto& pipeline = dynamic_cast<const CE::Rendering::OpenGL::Pipeline&>(*material->definition().pipeline);

    renderer.deinitialize();
    glfwMakeContextCurrent(selected);
    // The borrowed window/context remains alive: deletion cannot rely on its destruction.
    EXPECT_EQ(glIsProgram(program), GL_FALSE);
    EXPECT_EQ(glIsVertexArray(vao), GL_FALSE);
    EXPECT_EQ(glIsBuffer(buffer), GL_FALSE);
    EXPECT_EQ(glIsTexture(texture), GL_FALSE);
    EXPECT_EQ(glIsSampler(static_cast<GLuint>(sampler_id)), GL_FALSE);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    EXPECT_THROW(renderer.render(frame), CE::Exceptions::failed_operation);
    EXPECT_THROW(pipeline.bind_parameters(material->resolve({}, {}, {}, {})), CE::Exceptions::failed_operation);
    EXPECT_THROW(geometry->bind(), CE::Exceptions::failed_operation);
    EXPECT_THROW(image->bind(0), CE::Exceptions::failed_operation);
    EXPECT_THROW(sampler->bind(2), CE::Exceptions::failed_operation);
    glfwMakeContextCurrent(nullptr);
    engine.reset();
    EXPECT_THROW(pipeline.bind_parameters(material->resolve({}, {}, {}, {})), CE::Exceptions::failed_operation);
    EXPECT_THROW(geometry->bind(), CE::Exceptions::failed_operation);
    EXPECT_THROW(image->bind(0), CE::Exceptions::failed_operation);
    EXPECT_THROW(sampler->bind(2), CE::Exceptions::failed_operation);
    frame.recycle();
    std::weak_ptr<const CE::Rendering::Resources::Material> material_owner = material;
    std::weak_ptr<CE::Rendering::Geometry::Buffer2D> geometry_owner = geometry;
    std::weak_ptr<CE::Rendering::Resources::Image> image_owner = image;
    std::weak_ptr<const CE::Rendering::Sampling::Sampler> sampler_owner = sampler;
    std::thread release([material = std::move(material), geometry = std::move(geometry), image = std::move(image),
                         sampler = std::move(sampler)]() mutable {
        material.reset();
        geometry.reset();
        image.reset();
        sampler.reset();
    });
    release.join();
    EXPECT_TRUE(material_owner.expired());
    EXPECT_TRUE(geometry_owner.expired());
    EXPECT_TRUE(image_owner.expired());
    EXPECT_TRUE(sampler_owner.expired());
}

TEST(native_opengl, text_runs) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& renderer = dynamic_cast<CE::Rendering::OpenGL::Renderer&>(engine->renderer());
    renderer.initialize();
    {
        NativeShaderFiles files;
        auto overlay = engine->resources().create_image({{1, 1}, {204, 255, 128, 255}});
        const auto other = text_run_material(
            engine->resources(), files, overlay, CE::Rendering::Sampling::Filter::Linear, CE::Rendering::Pipelines::Blend::Additive
        );
        const auto fonts = text_run_fonts();
        CE::Text::LayoutOptions options;
        options.pixel_height = 16;
        options.maximum_width = 56;
        for (const auto sample : std::array<std::u8string_view, 2>{u8"AAZZAA", u8"e\u0301 \u4e2d\n\u05d0\u05d1\u05d2 123"}) {
            SCOPED_TRACE(std::string(reinterpret_cast<const char*>(sample.data()), sample.size()));
            const auto text = CE::Text::Rendering::upload(
                CE::Text::Rendering::prepare(
                    CE::Text::layout_text(fonts, {reinterpret_cast<const char*>(sample.data()), sample.size()}, options), {20, true}
                ),
                engine->resources()
            );
            ASSERT_GT(text.draws().size(), text.runs().size());
            ASSERT_GT(text.pages().size(), 1u);
            // Baking offsets regroups floating-point transforms. Avoid exact
            // pixel-center edge ties in this strict raster comparison; CPU tests
            // also CE::Memory::Ordering transformed positions and unchanged atlas coordinates.
            for (const auto filter : {CE::Rendering::Sampling::Filter::Nearest, CE::Rendering::Sampling::Filter::Linear}) {
                SCOPED_TRACE(static_cast<int>(filter));
                const auto main = text_run_material(engine->resources(), files, overlay, filter);
                for (const auto scale : std::array{0.65f, -0.65f, 0.0f}) {
                    SCOPED_TRACE(scale);
                    for (const bool clipped : {false, true}) {
                        SCOPED_TRACE(clipped);
                        CE::Rendering::DrawStyle2D style;
                        style.material = main;
                        style.scale = scale;
                        style.alpha = 0.4f;
                        style.parameters.emplace("tint", glm::vec4{1, 0.2f, 0.1f, 1});
                        style.model_matrix =
                            glm::translate(glm::mat4{1}, scale < 0 ? glm::vec3{55.125f, 45.125f, 0} : glm::vec3{8.125f, 36.125f, 0});
                        style.model_matrix = glm::rotate(style.model_matrix, 0.21f, {0, 0, 1});
                        if (clipped)
                            style.clip = CE::Rendering::ClipRegion2D{{7, 8, 54, 56}, 64, 64};
                        CE::Rendering::Frame glyphs, runs;
                        CE::Rendering::FrameWriter glyph_writer(glyphs), run_writer(runs);
                        retain_text_runs(glyph_writer, text, style, false);
                        retain_text_runs(run_writer, text, style, true);
                        style.material = other;
                        style.alpha = 0.7f;
                        style.parameters["tint"] = glm::vec4{0.1f, 0.8f, 1, 0.5f};
                        style.model_matrix = glm::translate(style.model_matrix, {3, -5, 0});
                        if (clipped)
                            style.clip = CE::Rendering::ClipRegion2D{{12, 10, 50, 52}, 64, 64};
                        retain_text_runs(glyph_writer, text, style, false, 4);
                        retain_text_runs(run_writer, text, style, true, 4);
                        NativeDrawTrace trace;
                        const auto original = draw_pixels(renderer, glyphs);
                        EXPECT_EQ(trace.draws, text.draws().size() * 2);
                        trace.draws = 0;
                        const auto positioned = draw_pixels(renderer, runs);
                        EXPECT_EQ(trace.draws, text.runs().size() * 2);
                        expect_same_pixels(original, positioned, scale != 0);
                    }
                }
            }
        }
    }
    renderer.maintain_resources();
    renderer.deinitialize();
}

TEST(native_opengl, text_run_lifetime) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& renderer = dynamic_cast<CE::Rendering::OpenGL::Renderer&>(engine->renderer());
    renderer.initialize();
    {
        NativeShaderFiles files;
        auto overlay = engine->resources().create_image({{1, 1}, {255, 255, 255, 255}});
        CE::Rendering::DrawStyle2D style;
        style.material = text_run_material(engine->resources(), files, overlay, CE::Rendering::Sampling::Filter::Linear);
        style.scale = 0.65f;
        style.alpha = 0.6f;
        style.model_matrix = glm::translate(glm::mat4{1}, {8, 24, 0});
        const auto fonts = text_run_fonts();
        auto active = std::make_shared<CE::Text::Rendering::Generation>(CE::Text::Rendering::upload(
            CE::Text::Rendering::prepare(CE::Text::layout_text(fonts, "AAA"), {.positioned_runs = true}), engine->resources()
        ));
        const std::weak_ptr<CE::Rendering::Geometry::Buffer2D> positioned = active->positioned_geometry();
        const std::weak_ptr<CE::Rendering::Geometry::Buffer2D> glyphs = active->pages()[0].geometry;
        const std::weak_ptr<CE::Rendering::Resources::Image> atlas = active->pages()[0].atlas;
        const std::weak_ptr<const CE::Rendering::Resources::Material> material = style.material;
        CE::Rendering::Frame retained;
        CE::Rendering::FrameWriter retained_writer(retained);
        retain_text_runs(retained_writer, *active, style, true);
        const auto before = draw_pixels(renderer, retained);
        active = std::make_shared<CE::Text::Rendering::Generation>(CE::Text::Rendering::upload(
            CE::Text::Rendering::prepare(CE::Text::layout_text(fonts, "BBB"), {.positioned_runs = true}), engine->resources()
        ));
        style.material = text_run_material(
            engine->resources(), files, overlay, CE::Rendering::Sampling::Filter::Nearest, CE::Rendering::Pipelines::Blend::Additive
        );
        CE::Rendering::Frame replacement;
        CE::Rendering::FrameWriter replacement_writer(replacement);
        retain_text_runs(replacement_writer, *active, style, true);
        EXPECT_TRUE(glyphs.expired());
        EXPECT_FALSE(positioned.expired());
        EXPECT_FALSE(atlas.expired());
        EXPECT_FALSE(material.expired());
        renderer.maintain_resources();
        const auto after = draw_pixels(renderer, retained);
        expect_same_pixels(before, after);
        static_cast<void>(draw_pixels(renderer, replacement));
        retained.recycle();
        EXPECT_TRUE(positioned.expired());
        EXPECT_TRUE(atlas.expired());
        EXPECT_TRUE(material.expired());
    }
    renderer.maintain_resources();
    renderer.deinitialize();
}

TEST(native_opengl, rotated_legacy_text) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& renderer = dynamic_cast<CE::Rendering::OpenGL::Renderer&>(engine->renderer());
    renderer.initialize();
    NativeShaderFiles files;
    const auto widths_file = files.directory / "widths.bin";
    std::array<short, CE::Text::Legacy::bitmap_character_count> widths;
    widths.fill(128);
    widths['A' - 32] = 64;
    {
        std::ofstream output(widths_file, std::ios::binary);
        output.write(reinterpret_cast<const char*>(widths.data()), sizeof(widths));
        ASSERT_TRUE(output.good());
    }
    auto pixels = legacy_font_pixels();
    auto& textures = CE::Assets::Images::Manager::get();
    const auto texture_key = files.directory / "whitefont.png";
    textures.upload(texture_key, pixels, engine->resources());
    auto font = std::make_shared<CE::Text::Legacy::BitmapFont>(CE::Text::Legacy::BitmapFont::load_ffont(widths_file, engine->resources()));
    const std::weak_ptr<CE::Text::Legacy::BitmapFont> font_owner = font;
    const std::weak_ptr<CE::Rendering::Geometry::Buffer2D> geometry_owner = font->glyph_geometry_handle();
    const std::weak_ptr<CE::Rendering::Resources::Image> image_owner = font->glyph_atlas_handle();
    const auto texture = bound_texture_id(*font->glyph_atlas_handle());
    GLint vao = 0, buffer = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    ASSERT_NE(vao, 0);
    ASSERT_NE(buffer, 0);
    auto material = legacy_font_material(engine->resources());
    CE::Rendering::DrawStyle2D style;
    style.material = material;
    style.scale = 16;
    style.model_matrix[3] = {12.5f, 12.5f, 0, 1};
    CE::Rendering::Frame upright, rotated, alternate, baseline, multiline;
    retain_legacy_text(upright, *font, "A A", style);
    style.model_matrix[0] = {0, 1, 0, 0};
    style.model_matrix[1] = {-1, 0, 0, 0};
    retain_legacy_text(rotated, *font, "A A", style);
    retain_legacy_text(alternate, *font, "A A", style, true);
    // Legacy newline advances 1/128 local units. Scale 128 makes its
    // quarter-turned displacement one pixel at the visible clipped edge.
    style.scale = 128;
    style.model_matrix[3] = {-51, 32.5f, 0, 1};
    retain_legacy_text(baseline, *font, "B", style);
    retain_legacy_text(multiline, *font, "A\nB", style);
    pixels.rgba.clear();
    std::filesystem::remove(widths_file);
    font.reset();
    style.material.reset();
    material.reset();
    textures.clear_assets();
    EXPECT_TRUE(font_owner.expired());
    EXPECT_FALSE(geometry_owner.expired());
    EXPECT_FALSE(image_owner.expired());
    EXPECT_FALSE(textures.contains(texture_key));

    const auto straight = draw_pixels(renderer, upright);
    EXPECT_EQ(straight.at(12, 18), (std::array<unsigned char, 4>{255, 0, 0, 255}));
    EXPECT_EQ(straight.at(12, 6), (std::array<unsigned char, 4>{63, 0, 0, 255}));
    EXPECT_EQ(straight.at(36, 18), (std::array<unsigned char, 4>{255, 0, 0, 255}));
    const auto turned = draw_pixels(renderer, rotated);
    EXPECT_EQ(turned.at(6, 12), (std::array<unsigned char, 4>{255, 0, 0, 255}));
    EXPECT_EQ(turned.at(18, 12), (std::array<unsigned char, 4>{63, 0, 0, 255}));
    EXPECT_EQ(turned.at(6, 36), (std::array<unsigned char, 4>{255, 0, 0, 255}));
    EXPECT_EQ(turned.at(12, 24), (std::array<unsigned char, 4>{0, 0, 0, 255}));
    const auto fancy = draw_pixels(renderer, alternate);
    EXPECT_EQ(fancy.at(6, 12), (std::array<unsigned char, 4>{0, 0, 255, 255}));
    EXPECT_EQ(fancy.at(18, 12), (std::array<unsigned char, 4>{0, 0, 63, 255}));
    EXPECT_EQ(fancy.at(6, 44), (std::array<unsigned char, 4>{0, 0, 255, 255}));
    EXPECT_EQ(fancy.at(12, 32), (std::array<unsigned char, 4>{0, 0, 0, 255}));
    const auto single = draw_pixels(renderer, baseline);
    EXPECT_EQ(single.at(13, 32), (std::array<unsigned char, 4>{0, 0, 0, 255}));
    const auto lines = draw_pixels(renderer, multiline);
    EXPECT_EQ(lines.at(13, 32)[0], 0);
    EXPECT_GE(lines.at(13, 32)[1], 64);
    EXPECT_EQ(lines.at(13, 32)[2], 0);
    EXPECT_EQ(lines.at(14, 32), (std::array<unsigned char, 4>{0, 0, 0, 255}));
    GLint program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    ASSERT_NE(program, 0);
    renderer.maintain_resources();
    EXPECT_EQ(glIsTexture(texture), GL_TRUE);
    EXPECT_EQ(glIsVertexArray(vao), GL_TRUE);
    EXPECT_EQ(glIsBuffer(buffer), GL_TRUE);
    EXPECT_EQ(glIsProgram(program), GL_TRUE);
    upright.recycle();
    rotated.recycle();
    alternate.recycle();
    baseline.recycle();
    multiline.recycle();
    EXPECT_TRUE(geometry_owner.expired());
    EXPECT_TRUE(image_owner.expired());
    renderer.maintain_resources();
    EXPECT_EQ(glIsTexture(texture), GL_FALSE);
    EXPECT_EQ(glIsVertexArray(vao), GL_FALSE);
    EXPECT_EQ(glIsBuffer(buffer), GL_FALSE);
    EXPECT_EQ(glIsProgram(program), GL_FALSE);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    renderer.deinitialize();
}

TEST(native_opengl, texture_row_order) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& renderer = dynamic_cast<CE::Rendering::OpenGL::Renderer&>(engine->renderer());
    renderer.initialize();
    const auto path = std::filesystem::path(CHERYL_TEST_FIXTURE_DIR) / "rgba-two-rows.png";
    const auto decoded = CE::Assets::Images::decode(path);
    const std::array<unsigned char, 24> top_down{
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255};
    const std::array<unsigned char, 24> bottom_up{
        0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255};
    ASSERT_EQ(decoded.size.width, 3u);
    ASSERT_EQ(decoded.size.height, 2u);
    ASSERT_EQ(decoded.rgba, (std::vector<unsigned char>(top_down.begin(), top_down.end())));
    const auto direct =
        std::make_shared<CE::Rendering::OpenGL::Texture>(renderer.resources(), path.string().c_str(), false, false, GL_CLAMP_TO_EDGE);
    const auto loaded = engine->resources().load_image(path);
    const auto prepared = engine->resources().create_image(decoded);
    const std::array<const CE::Rendering::Resources::Image*, 3> images{direct.get(), loaded.get(), prepared.get()};
    for (const CE::Rendering::Resources::Image* image : images) {
        image->bind(0);
        std::array<unsigned char, 24> observed{};
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, observed.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        EXPECT_EQ(observed, bottom_up);
        EXPECT_EQ(glGetError(), GL_NO_ERROR);
    }
    EXPECT_EQ(decoded.rgba, (std::vector<unsigned char>(top_down.begin(), top_down.end())));
    const std::array<unsigned char, 6> alpha{1, 2, 3, 4, 5, 6};
    const auto atlas = engine->resources().create_font_atlas(alpha, {3, 2});
    atlas->bind(0);
    GLint previous_pack = 0;
    glGetIntegerv(GL_PACK_ALIGNMENT, &previous_pack);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    std::array<unsigned char, 6> observed_alpha{};
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, observed_alpha.data());
    glPixelStorei(GL_PACK_ALIGNMENT, previous_pack);
    glBindTexture(GL_TEXTURE_2D, 0);
    EXPECT_EQ(observed_alpha, alpha);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    renderer.deinitialize();
}

namespace {
    struct NativeTickRecord {
        CE::Runtime::Simulation::UpdateKind kind;
        double delta;
        double dropped;
        double observed;
        std::vector<std::uint64_t> polls;
        std::vector<CE::Input::Events::Record> records;
        CE::Input::Actions::ButtonTickState observed_button;
    };

    struct NativeRuntimeGame : CE::Game::AbstractGame {
        CE::Runtime::Context& engine;
        const CE::Platform::FramebufferSize expected_framebuffer_size;
        NativeShaderFiles files;
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D> geometry;
        std::shared_ptr<const CE::Rendering::Resources::Material> material;
        std::vector<NativeTickRecord> ticks; // Simulation-owned until run() joins.
        mutable std::atomic<std::size_t> preparations{0};
        std::function<void()> after_update;
        std::function<void()> after_init;
        std::optional<CE::Input::Actions::Id> observed_action;

        explicit NativeRuntimeGame(CE::Runtime::Context& context)
        : engine(context), expected_framebuffer_size(context.display().active_window()->framebuffer_size()) {
            EXPECT_GT(expected_framebuffer_size.width, 0);
            EXPECT_GT(expected_framebuffer_size.height, 0);
        }
        void init() override {
            geometry = fullscreen_triangle(engine.resources());
            material = material_builder(files.stages(), {1, 0, 0, 1})(engine.resources());
            if (after_init)
                after_init();
        }
        void update(const CE::Game::TickContext& tick) override {
            ticks.push_back({tick.update_kind, tick.delta_seconds, tick.dropped_seconds, tick.observed_seconds(), {}, {}, {}});
            for (const auto& poll : tick.input.polls())
                ticks.back().polls.push_back(poll->poll());
            ticks.back().records.assign(tick.input.records().begin(), tick.input.records().end());
            if (observed_action)
                ticks.back().observed_button = tick.input.button(*observed_action);
            EXPECT_EQ(tick.framebuffer_size, expected_framebuffer_size);
            after_update();
        }
        void prepare_render_frame(CE::Rendering::FrameWriter& frame) const override {
            ++preparations;
            auto pass = frame.begin_pass(glm::mat4{1.0f}, glm::mat4{1.0f});
            pass.add({geometry, material, 0, 3, material->resolve({}, {}, {}, {})});
        }
        void deinit() override {
            material.reset();
            geometry.reset();
            EXPECT_EQ(glGetError(), GL_NO_ERROR);
        }
    };

    // Every operation still reaches the real GLFW context. Only the first
    // completed swap gets a controlled acceptance workload on its owner.
    class DelayedNativeSurface final : public CE::Rendering::OpenGL::Context {
        CE::Rendering::OpenGL::GLFWContext native_;
        bool delayed_ = false;
    public:
        std::function<void()> after_first_swap;
        std::size_t presentations = 0;
        explicit DelayedNativeSurface(CE::Platform::GLFW::Window& window)
        : native_(window, 0) {}
        void make_current() override { native_.make_current(); }
        void release_current() override { native_.release_current(); }
        bool is_current() const override { return native_.is_current(); }
        ProcAddress proc_address(const char* name) const override { return native_.proc_address(name); }
        void present() override {
            std::array<unsigned char, 4> pixel{};
            glReadBuffer(GL_BACK);
            glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            EXPECT_EQ(pixel, (std::array<unsigned char, 4>{255, 0, 0, 255}));
            EXPECT_EQ(glGetError(), GL_NO_ERROR);
            native_.present();
            ++presentations;
            if (!std::exchange(delayed_, true))
                after_first_swap();
        }
    };

    std::unique_ptr<CE::Runtime::Context> delayed_native_context(
        DelayedNativeSurface*& observed,
        std::unique_ptr<CE::Input::System> input = std::make_unique<CE::Platform::GLFW::InputSystem>()
    ) {
        auto display = std::make_unique<CE::Platform::GLFW::DisplaySystem>();
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
        auto* window = display->create_window(
            display->primary_monitor(), CE::Platform::WindowMode::NORMAL, 64, 64, "Cheryl delayed native presentation"
        );
        display->activate_window(*window);
        auto surface = std::make_unique<DelayedNativeSurface>(*window);
        observed = surface.get();
        auto renderer = std::make_unique<CE::Rendering::OpenGL::Renderer>(*surface);
        auto resources = std::make_unique<CE::Rendering::OpenGL::ResourceProvider>(*renderer);
        return std::make_unique<CE::Runtime::Context>(
            std::move(display), std::move(surface), std::move(renderer), std::move(resources), std::move(input)
        );
    }

    // Forward complete native State snapshots; no synthetic input samples.
    // This acceptance scope needs only State, so Events/Text remain on native_.
    class ObservedNativeInput final : public CE::Input::System {
        CE::Platform::GLFW::InputSystem native_;
        std::mutex mutex_;
        std::condition_variable progress_;
        std::uint64_t sequence_ = 0;
    public:
        std::function<void()> before_poll;
        std::function<void()> after_poll;
        bool deinitialized = false;
        void initialize(CE::Platform::Window& window) override { native_.initialize(window); }
        void poll() override {
            if (before_poll)
                before_poll();
            native_.poll();
            {
                std::lock_guard lock(mutex_);
                sequence_ = native_.action_snapshot()->poll();
            }
            progress_.notify_all();
            if (after_poll)
                after_poll();
        }
        void deinitialize() override {
            native_.deinitialize();
            deinitialized = true;
        }
        CE::Input::Actions::Bindings& bindings() override { return native_.bindings(); }
        std::shared_ptr<const CE::Input::Actions::Snapshot> action_snapshot() override { return native_.action_snapshot(); }
        std::shared_ptr<const CE::Input::PollSnapshot> poll_snapshot() override { return native_.poll_snapshot(); }
        CE::Input::DeviceId keyboard_id() const override { return native_.keyboard_id(); }
        CE::Input::DeviceId mouse_id() const override { return native_.mouse_id(); }
        CE::Input::DeviceId gamepad_id() const override { return native_.gamepad_id(); }
        CE::Platform::GLFW::InputSystem& native() { return native_; }
        bool wait_for_sequence(std::uint64_t target) {
            std::unique_lock lock(mutex_);
            return progress_.wait_for(lock, std::chrono::seconds{2}, [&] { return sequence_ >= target; });
        }
        std::uint64_t sequence() {
            std::lock_guard lock(mutex_);
            return sequence_;
        }
    };

    enum class NativeRuntimeFault { Initialization, PartialFrame, Presentation };

    struct NativeFailureResources {
        GLuint program = 0;
        GLint vao = 0;
        GLint buffer = 0;
        GLuint texture = 0;
        GLuint shutdown_upload = 0;
        std::size_t platform_completions = 0;
        std::thread::id platform_owner;
    };

    // The CPU job retains native owners without calling GL. Quiesce releases it
    // only after worker submissions close, so its upload needs the shutdown pump.
    struct NativeFailureGame final : NativeRuntimeGame {
        DelayedNativeSurface& surface;
        const NativeRuntimeFault fault;
        const std::shared_ptr<NativeFailureResources> observed = std::make_shared<NativeFailureResources>();
        std::shared_ptr<CE::Rendering::Resources::Image> image;
        std::weak_ptr<CE::Rendering::Geometry::Buffer2D> geometry_owner;
        std::weak_ptr<const CE::Rendering::Resources::Material> material_owner;
        std::weak_ptr<CE::Rendering::Resources::Image> image_owner;
        std::optional<CE::Workers::Group> jobs;
        std::future<int> completion;
        std::promise<void> release_cpu;
        std::size_t quiesces = 0;
        std::size_t deinits = 0;

        NativeFailureGame(CE::Runtime::Context& context, DelayedNativeSurface& presentation, const NativeRuntimeFault requested)
        : NativeRuntimeGame(context), surface(presentation), fault(requested) {
            after_update = [] {};
            surface.after_first_swap = [this] {
                if (fault == NativeRuntimeFault::Presentation)
                    throw CE::Exceptions::failed_operation(CE_HERE, "Original native runtime failure");
            };
        }
        void init() override {
            NativeRuntimeGame::init();
            observed->platform_owner = std::this_thread::get_id();
            geometry->bind();
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &observed->vao);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &observed->buffer);
            const auto& pipeline = dynamic_cast<const CE::Rendering::OpenGL::Pipeline&>(*material->definition().pipeline);
            pipeline.bind_parameters(material->resolve({}, {}, {}, {}));
            GLint program = 0;
            glGetIntegerv(GL_CURRENT_PROGRAM, &program);
            observed->program = static_cast<GLuint>(program);
            image = engine.resources().create_image(CE::Assets::Images::Decoded{{1, 1}, {255, 255, 255, 255}});
            observed->texture = bound_texture_id(*image);
            geometry_owner = geometry;
            material_owner = material;
            image_owner = image;
            auto allowed = release_cpu.get_future().share();
            jobs.emplace(engine.make_worker_group());
            completion = jobs->submit([allowed, platform = engine.platform_dispatcher().submission(), observed = observed,
                                          geometry = geometry, material = material, image = image]() mutable {
                allowed.wait();
                auto owned_cpu_data = std::make_unique<int>(42);
                auto uploaded = platform.submit([observed, owned_cpu_data = std::move(owned_cpu_data), geometry = std::move(geometry),
                                                 material = std::move(material), image = std::move(image)](CE::Runtime::Context& context) {
                    EXPECT_EQ(std::this_thread::get_id(), observed->platform_owner);
                    EXPECT_EQ(glIsProgram(observed->program), GL_TRUE);
                    EXPECT_EQ(glIsVertexArray(observed->vao), GL_TRUE);
                    EXPECT_EQ(glIsBuffer(observed->buffer), GL_TRUE);
                    EXPECT_EQ(glIsTexture(observed->texture), GL_TRUE);
                    auto shutdown_image = context.resources().create_image(CE::Assets::Images::Decoded{{1, 1}, {0, 255, 0, 255}});
                    observed->shutdown_upload = bound_texture_id(*shutdown_image);
                    EXPECT_EQ(glIsTexture(observed->shutdown_upload), GL_TRUE);
                    EXPECT_EQ(glGetError(), GL_NO_ERROR);
                    ++observed->platform_completions;
                    return *owned_cpu_data;
                });
                return uploaded.get();
            });
            if (fault == NativeRuntimeFault::Initialization)
                throw CE::Exceptions::failed_operation(CE_HERE, "Original native runtime failure");
        }
        void prepare_render_frame(CE::Rendering::FrameWriter& frame) const override {
            NativeRuntimeGame::prepare_render_frame(frame);
            if (fault == NativeRuntimeFault::PartialFrame)
                throw CE::Exceptions::failed_operation(CE_HERE, "Original native runtime failure");
        }
        void quiesce() override {
            ++quiesces;
            EXPECT_EQ(std::this_thread::get_id(), observed->platform_owner);
            if (jobs)
                EXPECT_FALSE(jobs->status().accepting);
            EXPECT_FALSE(geometry_owner.expired());
            EXPECT_FALSE(material_owner.expired());
            EXPECT_FALSE(image_owner.expired());
            release_cpu.set_value();
        }
        void deinit() override {
            ++deinits;
            EXPECT_EQ(std::this_thread::get_id(), observed->platform_owner);
            if (completion.valid())
                EXPECT_EQ(completion.wait_for(std::chrono::seconds{0}), std::future_status::ready);
            if (jobs)
                EXPECT_EQ(jobs->status().completed, 1u);
            EXPECT_EQ(observed->platform_completions, 1u);
            NativeRuntimeGame::deinit();
            image.reset();
            // Exercise real loss of the current binding, not a fake GL query.
            // Maintenance will reject it, then renderer shutdown must rebind its
            // owner context and delete resources despite this later hook error.
            surface.release_current();
            throw CE::Exceptions::failed_operation(CE_HERE, "Secondary native cleanup failure");
        }
    };
}

TEST(native_opengl, runtime_failure_cleanup) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    std::size_t exercised = 0;
    for (const auto mode : {CE::Runtime::RunMode::Sequential, CE::Runtime::RunMode::Concurrent}) {
        for (const auto fault : {NativeRuntimeFault::Initialization, NativeRuntimeFault::PartialFrame, NativeRuntimeFault::Presentation}) {
            SCOPED_TRACE(::testing::Message() << "mode=" << static_cast<int>(mode) << " fault=" << static_cast<int>(fault));
            DelayedNativeSurface* surface = nullptr;
            auto native_input = std::make_unique<ObservedNativeInput>();
            auto* input = native_input.get();
            auto engine = delayed_native_context(surface, std::move(native_input));
            NativeFailureGame game(*engine, *surface, fault);
            CE::Runtime::GameRuntime runtime(*engine, game, mode);
            try {
                runtime.run();
                ADD_FAILURE() << "The selected native runtime failure did not propagate";
            } catch (const CE::Exceptions::failed_operation& error) {
                EXPECT_NE(std::string_view(error.what()).find("Original native runtime failure"), std::string_view::npos);
                EXPECT_EQ(std::string_view(error.what()).find("Secondary native cleanup failure"), std::string_view::npos);
            }
            EXPECT_EQ(game.quiesces, 1u);
            EXPECT_EQ(game.deinits, 1u);
            ASSERT_TRUE(game.completion.valid());
            ASSERT_EQ(game.completion.wait_for(std::chrono::seconds{0}), std::future_status::ready);
            EXPECT_EQ(game.completion.get(), 42);
            EXPECT_EQ(game.jobs->status().accepted, 1u);
            EXPECT_EQ(game.jobs->status().completed, 1u);
            EXPECT_EQ(game.jobs->status().running, 0u);
            EXPECT_EQ(game.jobs->status().pending, 0u);
            EXPECT_TRUE(game.geometry_owner.expired());
            EXPECT_TRUE(game.material_owner.expired());
            EXPECT_TRUE(game.image_owner.expired());
            EXPECT_TRUE(input->deinitialized);
            EXPECT_FALSE(surface->is_current());
            EXPECT_EQ(surface->presentations, fault == NativeRuntimeFault::Presentation ? 1u : 0u);
            EXPECT_EQ(game.preparations.load() > 0, fault != NativeRuntimeFault::Initialization);
            EXPECT_THROW(static_cast<void>(engine->make_worker_group()), CE::Exceptions::failed_operation);
            EXPECT_THROW(
                static_cast<void>(engine->platform_dispatcher().submit([](CE::Runtime::Context&) {})), CE::Exceptions::failed_operation
            );
            surface->make_current();
            EXPECT_NE(game.observed->program, 0u);
            EXPECT_NE(game.observed->vao, 0);
            EXPECT_NE(game.observed->buffer, 0);
            EXPECT_NE(game.observed->texture, 0u);
            EXPECT_NE(game.observed->shutdown_upload, 0u);
            // The borrowed window is still alive; driver deletion cannot be
            // attributed to destroying its context at the end of the test.
            EXPECT_EQ(glIsProgram(game.observed->program), GL_FALSE);
            EXPECT_EQ(glIsVertexArray(game.observed->vao), GL_FALSE);
            EXPECT_EQ(glIsBuffer(game.observed->buffer), GL_FALSE);
            EXPECT_EQ(glIsTexture(game.observed->texture), GL_FALSE);
            EXPECT_EQ(glIsTexture(game.observed->shutdown_upload), GL_FALSE);
            EXPECT_EQ(glGetError(), GL_NO_ERROR);
            surface->release_current();
            ++exercised;
        }
    }
    ::testing::Test::RecordProperty("native_failure_scenarios", static_cast<int>(exercised));
}

TEST(native_opengl, slow_update_recovery) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    for (const auto mode : {CE::Runtime::RunMode::Sequential, CE::Runtime::RunMode::Concurrent}) {
        for (int policy = 0; policy < 4; ++policy) {
            SCOPED_TRACE(::testing::Message() << "run mode=" << static_cast<int>(mode) << " policy=" << policy);
            auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
            NativeRuntimeGame game(*engine);
            CE::Runtime::Simulation::TimingOptions timing;
            timing.variable_interval = std::chrono::milliseconds{5};
            timing.fixed_step = std::chrono::milliseconds{10};
            timing.max_fixed_updates = 2;
            timing.recovery_cap = std::chrono::milliseconds{15};
            if (policy != 0)
                timing.mode = CE::Runtime::Simulation::Mode::Fixed;
            if (policy >= 2) {
                timing.recovery = CE::Runtime::Simulation::LagRecovery::VariableCatchUp;
                timing.fixed_updates_before_recovery = policy == 2 ? 1 : 0;
            }
            CE::Runtime::GameRuntime runtime(*engine, game, mode, {}, timing);
            game.after_update = [&] {
                if (game.ticks.size() == 1) {
                    // This is deliberately slow work, not a sleep used to prove
                    // thread ordering. The next real-clock batch must recover it.
                    const auto deadline = CE::Runtime::Simulation::Clock::now() + std::chrono::milliseconds{80};
                    std::this_thread::sleep_until(deadline);
                }
                if (game.ticks.size() == 24)
                    runtime.stop();
            };
            ASSERT_NO_THROW(runtime.run());
            ASSERT_EQ(game.ticks.size(), 24u);
            ASSERT_GT(game.preparations.load(), 0u);
            bool saw_lag = false;
            bool saw_variable_lag = false;
            bool saw_recovery = false;
            bool saw_drop = false;
            for (const auto& tick : game.ticks) {
                saw_lag |= tick.observed >= 0.07;
                saw_variable_lag |= tick.delta >= 0.07;
                saw_drop |= tick.dropped > 0;
                if (policy == 0) {
                    EXPECT_EQ(tick.kind, CE::Runtime::Simulation::UpdateKind::Variable);
                    EXPECT_EQ(tick.dropped, 0);
                } else if (tick.kind == CE::Runtime::Simulation::UpdateKind::Fixed) {
                    EXPECT_DOUBLE_EQ(tick.delta, 0.01);
                } else {
                    EXPECT_GE(policy, 2);
                    EXPECT_EQ(tick.kind, CE::Runtime::Simulation::UpdateKind::VariableCatchUp);
                    EXPECT_LE(tick.delta, 0.015);
                    saw_recovery = true;
                }
            }
            EXPECT_TRUE(saw_lag);
            if (policy == 0)
                EXPECT_TRUE(saw_variable_lag);
            if (policy != 0)
                EXPECT_TRUE(saw_drop);
            if (policy >= 2)
                EXPECT_TRUE(saw_recovery);
        }
    }
}

TEST(native_opengl, slow_presentation) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    for (const auto mode : {CE::Runtime::RunMode::Sequential, CE::Runtime::RunMode::Concurrent}) {
        SCOPED_TRACE(static_cast<int>(mode));
        DelayedNativeSurface* surface = nullptr;
        auto engine = delayed_native_context(surface);
        NativeRuntimeGame game(*engine);
        CE::Runtime::Simulation::TimingOptions timing;
        timing.variable_interval = std::chrono::milliseconds{5};
        CE::Runtime::GameRuntime runtime(*engine, game, mode, {}, timing);
        std::mutex mutex;
        std::condition_variable progress;
        std::size_t updates = 0;
        std::size_t updates_during_hold = 0;
        std::size_t preparations_during_hold = 0;
        std::size_t stop_at = 200; // Finite fallback if no presentation occurs.
        bool hold_completed = false;
        game.after_update = [&] {
            bool stop;
            {
                std::lock_guard lock(mutex);
                ++updates;
                stop = updates >= stop_at;
            }
            progress.notify_all();
            if (stop)
                runtime.stop();
        };
        surface->after_first_swap = [&] {
            if (mode == CE::Runtime::RunMode::Sequential) {
                std::this_thread::sleep_until(CE::Runtime::Simulation::Clock::now() + std::chrono::milliseconds{80});
                std::lock_guard lock(mutex);
                stop_at = updates + 8;
                hold_completed = true;
            } else {
                std::unique_lock lock(mutex);
                const auto initial_updates = updates;
                const auto initial_preparations = game.preparations.load();
                // A predicate proves progress while the platform is held inside
                // presentation. The deadline only bounds a broken runtime.
                hold_completed = progress.wait_for(lock, std::chrono::seconds{2}, [&] { return updates >= initial_updates + 8; });
                updates_during_hold = updates - initial_updates;
                preparations_during_hold = game.preparations.load() - initial_preparations;
                stop_at = updates + 8;
            }
        };
        ASSERT_NO_THROW(runtime.run());
        EXPECT_TRUE(hold_completed);
        EXPECT_GT(surface->presentations, 1u);
        if (mode == CE::Runtime::RunMode::Concurrent) {
            RecordProperty("updates_during_presentation", static_cast<int>(updates_during_hold));
            RecordProperty("preparations_during_presentation", static_cast<int>(preparations_during_hold));
            EXPECT_GE(updates_during_hold, 8u);
            // Three slots include the presented frame. Occupied snapshots stop
            // new frame preparation, while authoritative updates continue.
            EXPECT_LE(preparations_during_hold, 2u);
            EXPECT_GT(updates_during_hold, preparations_during_hold);
        } else {
            bool saw_presentation_lag = false;
            for (const auto& tick : game.ticks)
                saw_presentation_lag |= tick.delta >= 0.07;
            EXPECT_TRUE(saw_presentation_lag);
        }
    }
}

TEST(native_opengl, polling_backpressure) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    for (const std::size_t capacity : {1u, 3u}) {
        SCOPED_TRACE(capacity);
        auto input = std::make_unique<ObservedNativeInput>();
        auto* observed_input = input.get();
        DelayedNativeSurface* surface = nullptr;
        auto engine = delayed_native_context(surface, std::move(input));
        surface->after_first_swap = [] {};
        NativeRuntimeGame game(*engine);
        CE::Input::PollingOptions polling;
        polling.policy = capacity == 1 ? CE::Input::PollingPolicy::Lockstep : CE::Input::PollingPolicy::Finite;
        polling.capacity = capacity;
        CE::Runtime::Simulation::TimingOptions timing;
        timing.variable_interval = std::chrono::milliseconds{5};
        CE::Runtime::GameRuntime runtime(*engine, game, CE::Runtime::RunMode::Concurrent, polling, timing);
        std::uint64_t last_consumed = 0;
        std::size_t first_presentations = 0;
        game.after_update = [&] {
            if (game.ticks.size() == 4) {
                ASSERT_FALSE(game.ticks.back().polls.empty());
                last_consumed = game.ticks.back().polls.back();
                ASSERT_TRUE(observed_input->wait_for_sequence(last_consumed + capacity));
                // Hold this update until two distinct platform drains finish.
                // With no consumption, a full batch must prevent more native
                // polls while the previously published frame keeps presenting.
                for (int drain = 0; drain < 2; ++drain) {
                    auto request = engine->platform_dispatcher().submit([&, drain](CE::Runtime::Context&) {
                        EXPECT_EQ(observed_input->sequence(), last_consumed + capacity);
                        if (drain == 0)
                            first_presentations = surface->presentations;
                        else
                            EXPECT_GT(surface->presentations, first_presentations);
                    });
                    ASSERT_EQ(request.wait_for(std::chrono::seconds{2}), std::future_status::ready);
                    request.get();
                }
            }
            if (game.ticks.size() == 5) {
                // The full backlog was consumed by this update. Wait for an
                // admitted replacement poll instead of assuming the platform
                // owner beats the next simulation deadline under host load.
                ASSERT_TRUE(observed_input->wait_for_sequence(last_consumed + capacity + 1));
                auto published = engine->platform_dispatcher().submit([](CE::Runtime::Context&) {});
                ASSERT_EQ(published.wait_for(std::chrono::seconds{2}), std::future_status::ready);
                published.get();
            }
            if (game.ticks.size() == 12)
                runtime.stop();
        };
        ASSERT_NO_THROW(runtime.run());
        ASSERT_EQ(game.ticks.size(), 12u);
        ASSERT_GT(first_presentations, 0u);
        ASSERT_EQ(game.ticks[4].polls.size(), capacity);
        for (std::size_t index = 0; index < capacity; ++index)
            EXPECT_EQ(game.ticks[4].polls[index], last_consumed + index + 1);
        EXPECT_GT(game.ticks[5].polls.size(), 0u);
    }
}

#if defined(__linux__) && defined(CHERYL_NATIVE_X11_TESTS)
namespace {
    // Send synthetic server events to this test's own window. XSync establishes
    // server receipt, while only the runtime's normal GLFW poll dispatches them.
    void send_x11_key(CE::Platform::GLFW::Window& window, const KeySym symbol, const int type, const unsigned state, Time& time) {
        auto* display = glfwGetX11Display();
        const auto target = glfwGetX11Window(window.native_handle());
        XEvent event{};
        event.xkey.type = type;
        event.xkey.display = display;
        event.xkey.window = target;
        event.xkey.root = DefaultRootWindow(display);
        event.xkey.same_screen = True;
        event.xkey.time = time;
        time += 30; // Distinct presses, outside legacy release/repeat filtering.
        event.xkey.state = state;
        event.xkey.keycode = XKeysymToKeycode(display, symbol);
        ASSERT_NE(event.xkey.keycode, 0u);
        ASSERT_NE(XSendEvent(display, target, False, KeyPressMask | KeyReleaseMask, &event), 0);
        XSync(display, False);
    }
}

TEST(native_opengl, x11_input_recovery) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real X11 GLFW display to run native acceptance";
    using namespace CE::Input;
    for (const auto mode : {CE::Runtime::RunMode::Sequential, CE::Runtime::RunMode::Concurrent}) {
        for (const auto recovery :
             {CE::Runtime::Simulation::LagRecovery::DropExcessLag, CE::Runtime::Simulation::LagRecovery::VariableCatchUp}) {
            SCOPED_TRACE(::testing::Message() << "mode=" << static_cast<int>(mode) << " recovery=" << static_cast<int>(recovery));
            auto input = std::make_unique<ObservedNativeInput>();
            auto* observed = input.get();
            DelayedNativeSurface* surface = nullptr;
            auto engine = delayed_native_context(surface, std::move(input));
            if (glfwGetPlatform() != GLFW_PLATFORM_X11)
                GTEST_SKIP() << "This native record case requires GLFW's X11 backend";
            surface->after_first_swap = [] {};
            auto& window = dynamic_cast<CE::Platform::GLFW::Window&>(engine->window());
            NativeRuntimeGame game(*engine);
            constexpr CE::Input::Actions::Id typing_action{91};
            game.observed_action = typing_action;
            PollingOptions polling;
            polling.policy = PollingPolicy::Finite;
            polling.capacity = 3;
            CE::Runtime::Simulation::TimingOptions timing;
            timing.mode = CE::Runtime::Simulation::Mode::Fixed;
            timing.fixed_step = std::chrono::milliseconds{10};
            timing.max_fixed_updates = 2;
            timing.recovery = recovery;
            timing.fixed_updates_before_recovery = 1;
            timing.recovery_cap = std::chrono::milliseconds{20};
            CE::Runtime::GameRuntime runtime(*engine, game, mode, polling, timing);
            CE::Input::Capture::Lease old_events, old_text, new_events, new_text;
            CE::Input::Routing::Lease old_focus, new_focus;
            std::uint64_t old_epoch = 0;
            Time event_time = 100;
            std::promise<void> full_verified;
            auto verified = full_verified.get_future();
            std::future<void> gate;
            game.after_init = [&] {
                auto& native = observed->native();
                for (const auto key : {KeyboardKey::A, KeyboardKey::B, KeyboardKey::C})
                    (void)native.bindings().bind_button({native.keyboard_id(), key}, typing_action);
                old_events = native.capture(CE::Input::Capture::Channel::Events);
                old_text = native.capture(CE::Input::Capture::Channel::Text);
                old_focus = native.routing().focus(7);
                old_epoch = old_focus.epoch();
                if (mode == CE::Runtime::RunMode::Concurrent)
                    gate = runtime.simulation_dispatcher().submit([&] {
                        // This runs before consumption. Holding simulation here lets
                        // native polls fill capacity even if the clock is already due.
                        if (verified.wait_for(std::chrono::seconds{2}) != std::future_status::ready)
                            throw CE::Exceptions::failed_operation(CE_HERE, "Native full-batch verification timed out");
                        verified.get();
                    });
            };
            observed->before_poll = [&] {
                const auto next = observed->sequence() + 1;
                if (next == 1) {
                    send_x11_key(window, XK_a, KeyPress, ShiftMask, event_time);
                    send_x11_key(window, XK_a, KeyPress, ShiftMask, event_time); // GLFW repeat plus another committed character.
                    send_x11_key(window, XK_a, KeyRelease, ShiftMask, event_time);
                } else if (next == 2) {
                    send_x11_key(window, XK_b, KeyPress, 0, event_time);
                    send_x11_key(window, XK_b, KeyRelease, 0, event_time);
                }
            };
            observed->after_poll = [&] {
                if (observed->sequence() != 3)
                    return;
                auto& native = observed->native();
                new_focus = native.routing().focus(42, CE::Input::Routing::KeyboardPolicy::PassThrough);
                old_focus.reset(); // Cannot clear the replacement owner.
                old_events.reset();
                old_text.reset();
                new_events = native.capture(CE::Input::Capture::Channel::Events);
                new_text = native.capture(CE::Input::Capture::Channel::Text);
                // The concurrent gate keeps this batch full. Server events wait
                // for resumed polling. Sequential mode tests the same native
                // record/focus transition between polls on the shared owner.
                send_x11_key(window, XK_c, KeyPress, 0, event_time);
                send_x11_key(window, XK_c, KeyRelease, 0, event_time);
                if (mode == CE::Runtime::RunMode::Concurrent)
                    auto first = engine->platform_dispatcher().submit([&](CE::Runtime::Context&) {
                        EXPECT_EQ(observed->sequence(), 3u);
                        auto second = engine->platform_dispatcher().submit([&](CE::Runtime::Context&) {
                            EXPECT_EQ(observed->sequence(), 3u);
                            full_verified.set_value();
                        });
                    });
            };
            game.after_update = [&] {
                if (game.ticks.size() == 1)
                    std::this_thread::sleep_until(CE::Runtime::Simulation::Clock::now() + std::chrono::milliseconds{120});
                if (game.ticks.size() == 16)
                    runtime.stop();
            };
            ASSERT_NO_THROW(runtime.run());
            if (gate.valid())
                ASSERT_NO_THROW(gate.get());
            ASSERT_EQ(game.ticks.size(), 16u);
            if (mode == CE::Runtime::RunMode::Concurrent) {
                ASSERT_EQ(game.ticks.front().polls.size(), 3u);
                ASSERT_EQ(game.ticks.front().records.size(), 8u);
            }
            std::vector<CE::Input::Events::Record> earlier;
            for (const auto& tick : game.ticks)
                earlier.insert(earlier.end(), tick.records.begin(), tick.records.end());
            ASSERT_EQ(earlier.size(), 11u);
            const std::array<CE::Input::Events::ButtonPhase, 5> phases{
                CE::Input::Events::ButtonPhase::Press, CE::Input::Events::ButtonPhase::Repeat, CE::Input::Events::ButtonPhase::Release,
                CE::Input::Events::ButtonPhase::Press, CE::Input::Events::ButtonPhase::Release
            };
            const std::array<std::size_t, 5> buttons{0, 2, 4, 5, 7};
            for (std::size_t i = 0; i < buttons.size(); ++i) {
                ASSERT_TRUE(std::holds_alternative<CE::Input::Events::Button>(earlier[buttons[i]].data));
                EXPECT_EQ(std::get<CE::Input::Events::Button>(earlier[buttons[i]].data).phase, phases[i]);
                EXPECT_EQ(std::get<CE::Input::Events::Button>(earlier[buttons[i]].data).native_code, i < 3 ? GLFW_KEY_A : GLFW_KEY_B);
                EXPECT_EQ(has_modifier(std::get<CE::Input::Events::Button>(earlier[buttons[i]].data).modifiers, Modifiers::Shift), i < 3);
            }
            EXPECT_EQ(std::get<CE::Input::Events::Text>(earlier[1].data).codepoint, U'A');
            EXPECT_EQ(std::get<CE::Input::Events::Text>(earlier[3].data).codepoint, U'A');
            EXPECT_EQ(std::get<CE::Input::Events::Text>(earlier[6].data).codepoint, U'b');
            ASSERT_TRUE(std::holds_alternative<CE::Input::Events::Button>(earlier[8].data));
            EXPECT_EQ(std::get<CE::Input::Events::Button>(earlier[8].data).phase, CE::Input::Events::ButtonPhase::Press);
            EXPECT_EQ(std::get<CE::Input::Events::Button>(earlier[8].data).native_code, GLFW_KEY_C);
            ASSERT_TRUE(std::holds_alternative<CE::Input::Events::Text>(earlier[9].data));
            EXPECT_EQ(std::get<CE::Input::Events::Text>(earlier[9].data).codepoint, U'c');
            ASSERT_TRUE(std::holds_alternative<CE::Input::Events::Button>(earlier[10].data));
            EXPECT_EQ(std::get<CE::Input::Events::Button>(earlier[10].data).phase, CE::Input::Events::ButtonPhase::Release);
            EXPECT_EQ(std::get<CE::Input::Events::Button>(earlier[10].data).native_code, GLFW_KEY_C);
            std::size_t delivered = 0, c_presses = 0, c_releases = 0;
            std::uint64_t sequence = 0;
            bool saw_drop = false, saw_recovery = false;
            CE::Input::ObservationClock::time_point previous_observation = CE::Input::ObservationClock::time_point::min();
            for (const auto& tick : game.ticks) {
                saw_drop |= tick.dropped > 0;
                saw_recovery |= tick.kind == CE::Runtime::Simulation::UpdateKind::VariableCatchUp;
                c_presses += tick.observed_button.press_count;
                c_releases += tick.observed_button.release_count;
                EXPECT_FALSE(tick.observed_button.held());
                for (const auto& record : tick.records) {
                    EXPECT_EQ(record.sequence, ++sequence);
                    EXPECT_GE(record.observed_at, previous_observation);
                    previous_observation = record.observed_at;
                    EXPECT_EQ(record.device, observed->keyboard_id());
                    if (delivered < 8) {
                        EXPECT_EQ(record.target, 7u);
                        EXPECT_EQ(record.focus_epoch, old_epoch);
                        EXPECT_FALSE(record.to_gameplay);
                    } else {
                        EXPECT_EQ(record.target, 42u);
                        EXPECT_EQ(record.focus_epoch, new_focus.epoch());
                        EXPECT_TRUE(record.to_gameplay);
                        if (const auto* text = std::get_if<CE::Input::Events::Text>(&record.data))
                            EXPECT_EQ(text->codepoint, U'c');
                    }
                    ++delivered;
                }
            }
            EXPECT_EQ(delivered, 11u); // Eight old-focus records, then one three-record C tap.
            EXPECT_EQ(c_presses, 1u);
            EXPECT_EQ(c_releases, 1u);
            EXPECT_TRUE(saw_drop);
            EXPECT_EQ(saw_recovery, recovery == CE::Runtime::Simulation::LagRecovery::VariableCatchUp);
            const auto prefix = mode == CE::Runtime::RunMode::Sequential ? "sequential_" : "concurrent_";
            RecordProperty(
                std::string(prefix) +
                    (recovery == CE::Runtime::Simulation::LagRecovery::DropExcessLag ? "drop_records" : "catchup_records"),
                static_cast<int>(delivered)
            );
        }
    }
}
#endif

TEST(native_opengl, input_reattach) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& input = dynamic_cast<CE::Platform::GLFW::InputSystem&>(engine->input());
    auto& window = engine->window();
    EXPECT_THROW(input.poll(), CE::Exceptions::failed_operation);
    input.initialize(window);
    input.initialize(window); // Repeated attachment must not initialize Gainput twice.
    const auto keyboard = input.keyboard_id();
    const auto mouse = input.mouse_id();
    const auto gamepad = input.gamepad_id();
    const auto keyboard_device = input.device(keyboard).identity;
    ASSERT_NE(keyboard_device, 0u);
    ASSERT_NE(input.device(mouse).identity, 0u);
    ASSERT_NE(input.device(gamepad).identity, 0u);
    EXPECT_NO_THROW(input.poll());
    const auto gainput_time = input.backend_time_ms();
    std::this_thread::sleep_for(std::chrono::milliseconds{2});
    EXPECT_NO_THROW(input.poll());
    EXPECT_GT(input.backend_time_ms(), gainput_time);
    input.deinitialize();
    input.deinitialize();
    EXPECT_THROW(input.poll(), CE::Exceptions::failed_operation);
    EXPECT_EQ(input.device(keyboard).identity, keyboard_device);
    input.initialize(window);
    EXPECT_EQ(input.keyboard_id(), keyboard);
    EXPECT_EQ(input.mouse_id(), mouse);
    EXPECT_EQ(input.gamepad_id(), gamepad);
    EXPECT_EQ(input.device(keyboard).identity, keyboard_device);
    EXPECT_NO_THROW(input.poll());
    // Context destruction detaches from the live window and releases Gainput once.
}

TEST(native_opengl, input_owner) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto* other_window = engine->display().create_window(engine->display().primary_monitor(), engine->window().mode(), 64, 64);
    ASSERT_NE(other_window, nullptr);
    auto first = std::make_unique<CE::Platform::GLFW::InputSystem>();
    CE::Platform::GLFW::InputSystem second;
    first->initialize(engine->window());
    EXPECT_THROW(second.initialize(*other_window), CE::Exceptions::failed_operation);
    {
        CE::Platform::GLFW::InputSystem rejected;
        EXPECT_THROW(rejected.initialize(*other_window), CE::Exceptions::failed_operation);
    }
    EXPECT_NO_THROW(first->poll());
    first->deinitialize();
    // Detachment retains devices and ownership. A rejected adapter must not Exit
    // the first adapter's backend, and the original can still reattach and poll.
    EXPECT_THROW(second.initialize(*other_window), CE::Exceptions::failed_operation);
    EXPECT_NO_THROW(first->initialize(engine->window()));
    EXPECT_NO_THROW(first->poll());
    first.reset();
    EXPECT_NO_THROW(second.initialize(*other_window));
    EXPECT_NO_THROW(second.poll());
}

TEST(native_opengl, input_window) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& input = dynamic_cast<CE::Platform::GLFW::InputSystem&>(engine->input());
    auto* other_window = engine->display().create_window(engine->display().primary_monitor(), engine->window().mode(), 64, 64);
    ASSERT_NE(other_window, nullptr);
    input.initialize(engine->window());
    const auto keyboard = input.keyboard_id();
    const auto device = input.device(keyboard).identity;
    input.deinitialize();
#if defined(_WIN32)
    EXPECT_THROW(input.initialize(*other_window), CE::Exceptions::failed_operation);
    EXPECT_NO_THROW(input.initialize(engine->window()));
#else
    EXPECT_NO_THROW(input.initialize(*other_window));
#endif
    EXPECT_EQ(input.keyboard_id(), keyboard);
    EXPECT_EQ(input.device(keyboard).identity, device);
    EXPECT_NO_THROW(input.poll());
}

namespace {
    struct ResizeRegistration {
        CE::Events::EventSystem& events;
        CE::Events::EventSystem::Registration registration;
        ~ResizeRegistration() { events.unregister_listener(registration); }
    };
}

TEST(native_opengl, close_intent) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& window = dynamic_cast<CE::Platform::GLFW::Window&>(engine->window());
    ASSERT_TRUE(window.supports_close_intent());
    EXPECT_FALSE(window.consume_close_intent());
    for (int intent = 0; intent != 2; ++intent) {
        glfwSetWindowShouldClose(window.native_handle(), GLFW_TRUE);
        EXPECT_TRUE(window.should_close());
        EXPECT_TRUE(window.consume_close_intent());
        EXPECT_FALSE(window.should_close());
        EXPECT_FALSE(window.consume_close_intent());
    }
}

TEST(native_opengl, shutdown_negotiation) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    for (const auto mode : {CE::Runtime::RunMode::Sequential, CE::Runtime::RunMode::Concurrent}) {
        auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
        auto& window = dynamic_cast<CE::Platform::GLFW::Window&>(engine->window());
        class CloseGame final : public CE::Game::AbstractGame {
            GLFWwindow* window_;
            const std::chrono::steady_clock::time_point deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        public:
            explicit CloseGame(GLFWwindow* window) : window_(window) {}
            void init() override { glfwSetWindowShouldClose(window_, GLFW_TRUE); }
            void quiesce() override {}
            void deinit() override {}
            void update(const CE::Game::TickContext&) override {
                if (std::chrono::steady_clock::now() >= deadline_)
                    throw std::runtime_error("Native shutdown negotiation watchdog");
            }
            void prepare_render_frame(CE::Rendering::FrameWriter&) const override {}
        } game(window.native_handle());
        CE::Runtime::GameRuntime runtime(*engine, game, mode);
        std::vector<std::uint64_t> intents;
        std::vector<std::uint64_t> requests;
        auto policy = runtime.set_shutdown_policy([&](const auto& request, CE::Runtime::ShutdownDecision decision) {
            requests.push_back(request.id);
            if (requests.size() == 1)
                EXPECT_TRUE(decision.cancel());
            else
                EXPECT_TRUE(decision.accept());
        });
        engine->events().register_listener(CE::Platform::window_close_requested_event, [&](const auto& event) {
            intents.push_back(event.intent);
            EXPECT_FALSE(window.should_close());
        });
        engine->events().register_listener(CE::Runtime::shutdown_request_cancelled_event, [&](const auto&) {
            glfwSetWindowShouldClose(window.native_handle(), GLFW_TRUE);
        });
        EXPECT_NO_THROW(runtime.run());
        ASSERT_EQ(intents.size(), 2u);
        EXPECT_NE(intents[0], intents[1]);
        ASSERT_EQ(requests.size(), 2u);
        EXPECT_NE(requests[0], requests[1]);
        EXPECT_FALSE(window.should_close());
        EXPECT_EQ(runtime.lifecycle().snapshot().cause, CE::Runtime::ShutdownCause::AcceptedRequest);
        EXPECT_EQ(runtime.lifecycle().snapshot().outcome, CE::Runtime::RuntimeOutcome::Completed);
    }
}

TEST(native_opengl, resize_events) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& window = dynamic_cast<CE::Platform::GLFW::Window&>(engine->window());
    auto* handle = window.native_handle();
    const auto size = window.framebuffer_size();
    const auto callback = glfwSetFramebufferSizeCallback(handle, nullptr);
    glfwSetFramebufferSizeCallback(handle, callback);
    ASSERT_NE(callback, nullptr);
    auto& events = CE::Events::EventSystem::get();
    std::vector<int> order;
    std::vector<CE::Platform::FramebufferSize> named_sizes;
    std::vector<CE::Platform::FramebufferSize> typed_sizes;
    bool nested = false;
    const CE::Platform::FramebufferSize outer{size.width + 3, size.height + 3};
    const CE::Platform::FramebufferSize inner{size.width + 4, size.height + 4};
    ResizeRegistration named{events, events.register_listener("window-resized", [&](std::any payload) {
                                 const auto event = std::any_cast<CE::Platform::WindowResized>(payload);
                                 EXPECT_EQ(event.window, &window);
                                 order.push_back(1);
                                 named_sizes.push_back(event.size);
                                 if (nested && event.size == outer)
                                     callback(handle, inner.width, inner.height);
                             })};
    ResizeRegistration typed{
        events, events.register_listener(CE::Platform::window_resized_event, [&](const CE::Platform::WindowResized& event) {
            EXPECT_EQ(event.window, &window);
            order.push_back(2);
            typed_sizes.push_back(event.size);
        })
    };
    EXPECT_NO_THROW(callback(handle, size.width, size.height));
    EXPECT_TRUE(order.empty());
    EXPECT_NO_THROW(callback(handle, size.width + 1, size.height + 1));
    EXPECT_EQ(order, (std::vector<int>{1, 2}));
    EXPECT_EQ(named_sizes, typed_sizes);
    EXPECT_EQ(typed_sizes, (std::vector<CE::Platform::FramebufferSize>{{size.width + 1, size.height + 1}}));

    order.clear();
    named_sizes.clear();
    typed_sizes.clear();
    nested = true;
    EXPECT_NO_THROW(callback(handle, outer.width, outer.height));
    EXPECT_NO_THROW(window.check_native_failure());
    EXPECT_EQ(window.framebuffer_size(), inner);
    EXPECT_EQ(order, (std::vector<int>{1, 1, 2, 2}));
    EXPECT_EQ(named_sizes, (std::vector<CE::Platform::FramebufferSize>{outer, inner}));
    EXPECT_EQ(typed_sizes, (std::vector<CE::Platform::FramebufferSize>{inner, outer}));
}

TEST(native_opengl, typed_resize_failure) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& window = dynamic_cast<CE::Platform::GLFW::Window&>(engine->window());
    auto* handle = window.native_handle();
    const auto size = window.framebuffer_size();
    auto& events = CE::Events::EventSystem::get();
    int calls = 0;
    ResizeRegistration listener{
        events, events.register_listener(CE::Platform::window_resized_event, [&](const CE::Platform::WindowResized& event) {
            EXPECT_EQ(event.window, &window);
            EXPECT_EQ(event.size, window.framebuffer_size());
            ++calls;
            throw std::runtime_error("typed resize listener failed");
        })
    };
    const auto callback = glfwSetFramebufferSizeCallback(handle, nullptr);
    glfwSetFramebufferSizeCallback(handle, callback);
    ASSERT_NE(callback, nullptr);
    EXPECT_NO_THROW(callback(handle, size.width + 1, size.height + 1));
    EXPECT_NO_THROW(callback(handle, size.width + 2, size.height + 2));
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(window.framebuffer_size(), (CE::Platform::FramebufferSize{size.width + 2, size.height + 2}));
    EXPECT_THROW(window.check_native_failure(), std::runtime_error);
    EXPECT_NO_THROW(window.check_native_failure());
    EXPECT_NO_THROW(callback(handle, size.width + 3, size.height + 3));
    EXPECT_THROW(window.resize(96, 80), std::runtime_error);
    EXPECT_NO_THROW(window.check_native_failure());
}

TEST(native_opengl, resize_callback_failure) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Application::GLFWOpenGL::make_context(small_window());
    auto& window = dynamic_cast<CE::Platform::GLFW::Window&>(engine->window());
    auto* handle = window.native_handle();
    const auto size = window.framebuffer_size();
    auto& events = CE::Events::EventSystem::get();
    int calls = 0;
    int typed_calls = 0;
    ResizeRegistration listener{events, events.register_listener("window-resized", [&](std::any) {
                                    ++calls;
                                    throw std::runtime_error("resize listener failed");
                                })};
    ResizeRegistration typed{events, events.register_listener(CE::Platform::window_resized_event, [&](const CE::Platform::WindowResized&) {
                                 ++typed_calls;
                             })};
    // Invoke the actual registered C callback deterministically, without asking
    // the window manager to deliver a resize at a particular point in the test.
    const auto callback = glfwSetFramebufferSizeCallback(handle, nullptr);
    glfwSetFramebufferSizeCallback(handle, callback);
    ASSERT_NE(callback, nullptr);
    EXPECT_NO_THROW(callback(handle, size.width + 1, size.height + 1));
    EXPECT_NO_THROW(callback(handle, size.width + 2, size.height + 2));
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(window.framebuffer_size(), (CE::Platform::FramebufferSize{size.width + 2, size.height + 2}));
    EXPECT_THROW(window.check_native_failure(), std::runtime_error);
    EXPECT_NO_THROW(window.check_native_failure());

    EXPECT_NO_THROW(callback(handle, size.width + 3, size.height + 3));
    EXPECT_THROW(window.resize(96, 80), std::runtime_error);
    EXPECT_NO_THROW(window.check_native_failure());

    engine->input().initialize(window);
    EXPECT_NO_THROW(callback(handle, size.width + 4, size.height + 4));
    auto& input = dynamic_cast<CE::Platform::GLFW::InputSystem&>(engine->input());
    EXPECT_THROW(input.update(), std::runtime_error);
    EXPECT_NO_THROW(window.check_native_failure());
    input.deinitialize();
    EXPECT_EQ(typed_calls, 0);
}

#endif
