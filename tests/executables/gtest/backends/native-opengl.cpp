#ifndef CHERYL_SANDBOX_BUILD

#include <gtest/gtest.h>
#include <backends/opengl/glfw-backend.h>
#include <backends/opengl/renderer.h>
#include <backends/opengl/pipeline.h>
#include <assets/resources/resource-provider.h>
#include <assets/submission/draw2d.h>
#include <assets/types/2d/ffont.h>
#include <backends/opengl/resource-provider.h>
#include <backends/opengl/texture.h>
#include <core/display/window.h>
#include <core/rendering/render-frame.h>
#include <core/resources/asset-management/material-mgr.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <ext/matrix_clip_space.hpp>
#include <internals/exceptions.h>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {
    bool native_checks_requested() {
        const auto* value = std::getenv("CHERYL_NATIVE_GL_TESTS");
        return value && std::string_view(value) == "1";
    }

    CE::Engine::GlfwOpenGLConfig small_window() {
        CE::Engine::GlfwOpenGLConfig config;
        config.width = 64;
        config.height = 64;
        config.swap_interval = 0;
        config.title = "Cheryl native acceptance";
        return config;
    }

    GLuint bound_texture_id(
        const CE::Assets::Image& image
    ) {
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
        NativeShaderFiles(
            const NativeShaderFiles&
        ) = delete;
        NativeShaderFiles& operator=(
            const NativeShaderFiles&
        ) = delete;
        static void write(
            const std::filesystem::path& path,
            const std::string_view source
        ) {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            output << source;
            output.close();
            if (!output)
                throw CE::Exceptions::failed_operation(CE_HERE, "Could not write native shader test source");
        }
        [[nodiscard]] std::vector<std::filesystem::path> stages() const { return {vertex, fragment}; }
    };

    CE::Assets::MaterialMgr::Builder material_builder(
        std::vector<std::filesystem::path> stages,
        const glm::vec4 color,
        const bool missing_uniform = false
    ) {
        return [stages = std::move(stages), color, missing_uniform](CE::Assets::ResourceProvider& resources) {
            auto& native = dynamic_cast<CE::Assets::OpenGLResourceProvider&>(resources);
            CE::Assets::PipelineDefinition definition;
            definition.program_sources = stages;
            definition.parameters = {{"color", CE::Assets::ParameterType::Vec4}};
            CE::Assets::GLSLPipelineBindings bindings;
            bindings.parameters = {{"color", missing_uniform ? "absent_uniform" : "u_color", std::nullopt}};
            return native.build_material({native.build_pipeline(std::move(definition), bindings), {{"color", color}}});
        };
    }

    std::shared_ptr<CE::Assets::Geometry2D> fullscreen_triangle(
        CE::Assets::ResourceProvider& resources
    ) {
        const std::array<CE::Vertex2D, 3> vertices{
            {{-1.0f, -1.0f, 0.0f, 0.0f, 0.0f}, {3.0f, -1.0f, 0.0f, 0.0f, 0.0f}, {-1.0f, 3.0f, 0.0f, 0.0f, 0.0f}}};
        return resources.upload_geometry(vertices, CE::Assets::PrimitiveTopology::Triangles);
    }

    void retain_draw(
        CE::RenderAPIs::RenderFrame& frame,
        const std::shared_ptr<CE::Assets::Geometry2D>& geometry,
        const std::shared_ptr<const CE::Assets::Material>& material
    ) {
        CE::RenderAPIs::RenderFrameWriter writer(frame);
        auto pass = writer.begin_pass(glm::mat4{1.0f}, glm::mat4{1.0f});
        pass.add({geometry, material, 0, 3, material->resolve({}, {}, {}, {})});
    }

    std::array<unsigned char, 4> draw_pixel(
        CE::RenderAPIs::OpenGLRenderer& renderer,
        const CE::RenderAPIs::RenderFrame& frame
    ) {
        renderer.set_viewport({64, 64});
        renderer.clear();
        renderer.render(frame);
        std::array<unsigned char, 4> pixel{};
        glReadBuffer(GL_BACK);
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        EXPECT_EQ(glGetError(), GL_NO_ERROR);
        return pixel;
    }

    CE::Assets::DecodedImage legacy_font_pixels() {
        // Top-to-bottom source rows, with distinct banks and asymmetric glyphs.
        CE::Assets::DecodedImage image{{128, 128}, std::vector<unsigned char>(128 * 128 * 4, 0)};
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

    std::shared_ptr<const CE::Assets::Material> legacy_font_material(
        CE::Assets::ResourceProvider& resources
    ) {
        using namespace CE::Assets;
        auto& native = dynamic_cast<OpenGLResourceProvider&>(resources);
        const auto shaders = std::filesystem::path(CHERYL_SOURCE_DIR) / "assets/shaders/shader2d";
        PipelineDefinition definition;
        definition.program_sources = {shaders.string() + ".vert", shaders.string() + ".frag"};
        definition.parameters = {{"projection", ParameterType::Mat4, true, ParameterSemantic::Projection},
            {"view", ParameterType::Mat4, true, ParameterSemantic::View}, {"model", ParameterType::Mat4, true, ParameterSemantic::Model},
            {"alpha", ParameterType::Float, true, ParameterSemantic::Alpha},
            {"scale", ParameterType::Float, true, ParameterSemantic::Scale}, {"image", ParameterType::Sampler2D}};
        const GLSLPipelineBindings bindings{{{"projection", "projectionMatrix"}, {"view", "viewMatrix"}, {"model", "modelMatrix"},
            {"alpha", "in_Alpha"}, {"scale", "in_Scale"}, {"image", "mytexture"}}};
        return native.build_material({native.build_pipeline(std::move(definition), bindings), {}});
    }

    void retain_legacy_text(
        CE::RenderAPIs::RenderFrame& frame,
        const CE::Assets::FFont& font,
        const std::string_view text,
        const CE::RenderAPIs::DrawStyle2D& style,
        const bool alternate = false
    ) {
        const auto projection = glm::ortho(0.0f, 64.0f, 0.0f, 64.0f, -1.0f, 1.0f);
        CE::Assets::SubmissionContext2D context;
        context.pass = {projection, glm::mat4{1.0f}};
        context.image = CE::Assets::ImageParameter2D{"image", 3};
        CE::RenderAPIs::RenderFrameWriter writer(frame);
        auto pass = writer.begin_pass(projection, glm::mat4{1.0f});
        pass.add(CE::Assets::resolve_text(font, text, style, context, {alternate}));
    }

    struct NativeFramePixels {
        std::array<unsigned char, 64 * 64 * 4> rgba{};
        std::array<unsigned char, 4> at(
            const std::size_t x,
            const std::size_t y
        ) const {
            const auto offset = (y * 64 + x) * 4;
            return {rgba[offset], rgba[offset + 1], rgba[offset + 2], rgba[offset + 3]};
        }
    };

    NativeFramePixels draw_pixels(
        CE::RenderAPIs::OpenGLRenderer& renderer,
        const CE::RenderAPIs::RenderFrame& frame
    ) {
        renderer.set_viewport({64, 64});
        renderer.clear();
        renderer.render(frame);
        NativeFramePixels result;
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, result.rgba.data());
        EXPECT_EQ(glGetError(), GL_NO_ERROR);
        return result;
    }

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
        NativeCreationTrace(
            const NativeCreationTrace&
        ) = delete;
        NativeCreationTrace& operator=(
            const NativeCreationTrace&
        ) = delete;
        static GLuint GLAD_API_PTR create_program() {
            const auto id = active->original_program();
            if (active->program_count < active->programs.size())
                active->programs[active->program_count++] = id;
            else
                active->overflow = true;
            return id;
        }
        static GLuint GLAD_API_PTR create_shader(
            const GLenum kind
        ) {
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
        void expect_deleted(
            const std::size_t expected_stages
        ) const {
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

TEST(
    native_opengl,
    foreign_release_waits_for_owner_maintenance_without_drawing
) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Engine::make_glfw_opengl_context(small_window());
    auto& renderer = dynamic_cast<CE::RenderAPIs::OpenGLRenderer&>(engine->renderer());
    renderer.initialize();
    auto image = engine->resources().create_image(CE::Assets::DecodedImage{{1, 1}, {255, 255, 255, 255}});
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

TEST(
    native_opengl,
    another_current_context_rejects_use_and_shutdown_restores_the_owner
) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Engine::make_glfw_opengl_context(small_window());
    auto& renderer = dynamic_cast<CE::RenderAPIs::OpenGLRenderer&>(engine->renderer());
    renderer.initialize();
    auto image = engine->resources().create_image(CE::Assets::DecodedImage{{1, 1}, {255, 255, 255, 255}});
    const auto id = bound_texture_id(*image);
    auto* selected = dynamic_cast<CE::Window&>(engine->window()).native_handle();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> other(glfwCreateWindow(32, 32, "Other native context", nullptr, nullptr),
        glfwDestroyWindow);
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

TEST(
    native_opengl,
    retained_frames_survive_reload_and_cache_clear_until_owner_recycling
) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Engine::make_glfw_opengl_context(small_window());
    auto& renderer = dynamic_cast<CE::RenderAPIs::OpenGLRenderer&>(engine->renderer());
    renderer.initialize();
    NativeShaderFiles files;
    auto& materials = CE::Assets::MaterialMgr::get();
    const auto key = files.directory / "material";
    materials.load_material(key, engine->resources(), material_builder(files.stages(), {1.0f, 0.0f, 0.0f, 1.0f}));
    auto old = materials.get_asset(key);
    auto geometry = fullscreen_triangle(engine->resources());
    std::weak_ptr<const CE::Assets::Material> old_owner = old;
    std::weak_ptr<CE::Assets::Geometry2D> geometry_owner = geometry;
    GLint vao = 0, buffer = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    ASSERT_NE(vao, 0);
    ASSERT_NE(buffer, 0);
    CE::RenderAPIs::RenderFrame old_frame, new_frame;
    retain_draw(old_frame, geometry, old);
    EXPECT_EQ(draw_pixel(renderer, old_frame), (std::array<unsigned char, 4>{255, 0, 0, 255}));
    GLint old_program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);

    // Keep the authored color red; only the new shader swaps red and green.
    NativeShaderFiles::write(files.fragment, reloaded_fragment);
    materials.reload_material(key, engine->resources(), material_builder(files.stages(), {1.0f, 0.0f, 0.0f, 1.0f}));
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

TEST(
    native_opengl,
    failed_reload_preserves_draws_and_deletes_partial_or_retired_candidates
) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Engine::make_glfw_opengl_context(small_window());
    auto& renderer = dynamic_cast<CE::RenderAPIs::OpenGLRenderer&>(engine->renderer());
    renderer.initialize();
    NativeShaderFiles files;
    auto& materials = CE::Assets::MaterialMgr::get();
    const auto key = files.directory / "material";
    const auto recipe = material_builder(files.stages(), {1.0f, 0.0f, 0.0f, 1.0f});
    materials.load_material(key, engine->resources(), recipe);
    const auto published = materials.get_asset(key);
    auto geometry = fullscreen_triangle(engine->resources());
    CE::RenderAPIs::RenderFrame frame;
    retain_draw(frame, geometry, published);
    NativeCreationTrace trace;

    NativeShaderFiles::write(files.fragment, "#version 330 core\nthis is not valid GLSL;\n");
    EXPECT_THROW(materials.reload_material(key, engine->resources(), recipe), CE::Exceptions::runtime_exception);
    trace.expect_deleted(2);
    EXPECT_EQ(materials.get_asset(key), published);
    EXPECT_EQ(draw_pixel(renderer, frame), (std::array<unsigned char, 4>{255, 0, 0, 255}));

    trace.reset();
    NativeShaderFiles::write(files.fragment, mismatched_fragment);
    EXPECT_THROW(materials.reload_material(key, engine->resources(), recipe), CE::Exceptions::runtime_exception);
    trace.expect_deleted(2);
    EXPECT_EQ(materials.get_asset(key), published);
    EXPECT_EQ(draw_pixel(renderer, frame), (std::array<unsigned char, 4>{255, 0, 0, 255}));

    trace.reset();
    std::filesystem::remove(files.fragment);
    EXPECT_THROW(materials.reload_material(key, engine->resources(), recipe), CE::Exceptions::runtime_exception);
    trace.expect_deleted(1);
    EXPECT_EQ(materials.get_asset(key), published);
    EXPECT_EQ(draw_pixel(renderer, frame), (std::array<unsigned char, 4>{255, 0, 0, 255}));

    trace.reset();
    NativeShaderFiles::write(files.fragment, fragment_source);
    EXPECT_THROW(materials.reload_material(key, engine->resources(), material_builder(files.stages(), {0.0f, 1.0f, 0.0f, 1.0f}, true)),
        CE::Exceptions::invalid_args);
    ASSERT_EQ(trace.program_count, 1u);
    EXPECT_EQ(glIsProgram(trace.programs[0]), GL_TRUE); // Linked candidate retired after reflection rejection.
    renderer.maintain_resources();
    trace.expect_deleted(2);
    EXPECT_EQ(materials.get_asset(key), published);
    EXPECT_EQ(draw_pixel(renderer, frame), (std::array<unsigned char, 4>{255, 0, 0, 255}));

    trace.reset();
    materials.reload_material(key, engine->resources(), recipe);
    EXPECT_NE(materials.get_asset(key), published);
    EXPECT_EQ(draw_pixel(renderer, frame), (std::array<unsigned char, 4>{255, 0, 0, 255}));
    frame.recycle();
    renderer.deinitialize();
}

TEST(
    native_opengl,
    shutdown_deletes_bound_frame_resources_before_late_foreign_release
) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    auto engine = CE::Engine::make_glfw_opengl_context(small_window());
    auto& renderer = dynamic_cast<CE::RenderAPIs::OpenGLRenderer&>(engine->renderer());
    renderer.initialize();
    NativeShaderFiles files;
    auto material = material_builder(files.stages(), {1.0f, 0.0f, 0.0f, 1.0f})(engine->resources());
    auto geometry = fullscreen_triangle(engine->resources());
    auto image = engine->resources().create_image(CE::Assets::DecodedImage{{1, 1}, {255, 255, 255, 255}});
    const auto texture = bound_texture_id(*image);
    CE::RenderAPIs::RenderFrame frame;
    retain_draw(frame, geometry, material);
    EXPECT_EQ(draw_pixel(renderer, frame), (std::array<unsigned char, 4>{255, 0, 0, 255}));
    GLint program = 0, vao = 0, buffer = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    ASSERT_NE(program, 0);
    ASSERT_NE(vao, 0);
    ASSERT_NE(buffer, 0);
    auto* selected = dynamic_cast<CE::Window&>(engine->window()).native_handle();
    const auto& pipeline = dynamic_cast<const CE::Assets::GLSLPipeline&>(*material->definition().pipeline);

    renderer.deinitialize();
    glfwMakeContextCurrent(selected);
    // The borrowed window/context remains alive: deletion cannot rely on its destruction.
    EXPECT_EQ(glIsProgram(program), GL_FALSE);
    EXPECT_EQ(glIsVertexArray(vao), GL_FALSE);
    EXPECT_EQ(glIsBuffer(buffer), GL_FALSE);
    EXPECT_EQ(glIsTexture(texture), GL_FALSE);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    EXPECT_THROW(renderer.render(frame), CE::Exceptions::failed_operation);
    EXPECT_THROW(pipeline.bind_parameters(material->resolve({}, {}, {}, {})), CE::Exceptions::failed_operation);
    EXPECT_THROW(geometry->bind(), CE::Exceptions::failed_operation);
    EXPECT_THROW(image->bind(0), CE::Exceptions::failed_operation);
    glfwMakeContextCurrent(nullptr);
    engine.reset();
    EXPECT_THROW(pipeline.bind_parameters(material->resolve({}, {}, {}, {})), CE::Exceptions::failed_operation);
    EXPECT_THROW(geometry->bind(), CE::Exceptions::failed_operation);
    EXPECT_THROW(image->bind(0), CE::Exceptions::failed_operation);
    frame.recycle();
    std::weak_ptr<const CE::Assets::Material> material_owner = material;
    std::weak_ptr<CE::Assets::Geometry2D> geometry_owner = geometry;
    std::weak_ptr<CE::Assets::Image> image_owner = image;
    std::thread release([material = std::move(material), geometry = std::move(geometry), image = std::move(image)]() mutable {
        material.reset();
        geometry.reset();
        image.reset();
    });
    release.join();
    EXPECT_TRUE(material_owner.expired());
    EXPECT_TRUE(geometry_owner.expired());
    EXPECT_TRUE(image_owner.expired());
}

TEST(
    native_opengl,
    rotated_legacy_font_banks_widths_lines_and_retained_resources_match_pixels
) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    using namespace CE::Assets;
    using namespace CE::RenderAPIs;
    auto engine = CE::Engine::make_glfw_opengl_context(small_window());
    auto& renderer = dynamic_cast<OpenGLRenderer&>(engine->renderer());
    renderer.initialize();
    NativeShaderFiles files;
    const auto widths_file = files.directory / "widths.bin";
    std::array<short, num_chars_ffont> widths;
    widths.fill(128);
    widths['A' - 32] = 64;
    {
        std::ofstream output(widths_file, std::ios::binary);
        output.write(reinterpret_cast<const char*>(widths.data()), sizeof(widths));
        ASSERT_TRUE(output.good());
    }
    auto pixels = legacy_font_pixels();
    auto& textures = TextureMgr::get();
    const auto texture_key = files.directory / "whitefont.png";
    textures.load_asset(texture_key, pixels, engine->resources());
    auto font = std::make_shared<FFont>(FFont::load_ffont(widths_file, engine->resources()));
    const std::weak_ptr<FFont> font_owner = font;
    const std::weak_ptr<Geometry2D> geometry_owner = font->glyph_geometry_handle();
    const std::weak_ptr<Image> image_owner = font->glyph_atlas_handle();
    const auto texture = bound_texture_id(*font->glyph_atlas_handle());
    GLint vao = 0, buffer = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    ASSERT_NE(vao, 0);
    ASSERT_NE(buffer, 0);
    auto material = legacy_font_material(engine->resources());
    DrawStyle2D style;
    style.material = material;
    style.scale = 16;
    style.model_matrix[3] = {12.5f, 12.5f, 0, 1};
    RenderFrame upright, rotated, alternate, baseline, multiline;
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

TEST(
    native_opengl,
    rgba_file_and_provider_rows_flip_while_stb_alpha_rows_keep_their_order
) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    using namespace CE::Assets;
    auto engine = CE::Engine::make_glfw_opengl_context(small_window());
    auto& renderer = dynamic_cast<CE::RenderAPIs::OpenGLRenderer&>(engine->renderer());
    renderer.initialize();
    const auto path = std::filesystem::path(CHERYL_SOURCE_DIR) / "tests/fixtures/rgba-two-rows.png";
    const auto decoded = decode_image(path);
    const std::array<unsigned char, 24> top_down{
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255};
    const std::array<unsigned char, 24> bottom_up{
        0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255};
    ASSERT_EQ(decoded.size.width, 3u);
    ASSERT_EQ(decoded.size.height, 2u);
    ASSERT_EQ(decoded.rgba, (std::vector<unsigned char>(top_down.begin(), top_down.end())));
    const auto direct = std::make_shared<Texture>(renderer.resources(), path.string().c_str(), false, false, GL_CLAMP_TO_EDGE);
    const auto loaded = engine->resources().load_image(path);
    const auto prepared = engine->resources().create_image(decoded);
    const std::array<const Image*, 3> images{direct.get(), loaded.get(), prepared.get()};
    for (const Image* image : images) {
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

#endif
