#ifndef CHERYL_SANDBOX_BUILD

#include <gtest/gtest.h>
#include <backends/opengl/glfw-backend.h>
#include <backends/opengl/glfw-context.h>
#include <backends/opengl/renderer.h>
#include <backends/opengl/pipeline.h>
#include <assets/resources/resource-provider.h>
#include <assets/submission/draw2d.h>
#include <assets/types/2d/ffont.h>
#include <backends/opengl/resource-provider.h>
#include <backends/opengl/texture.h>
#include <core/display/window.h>
#include <core/display/display-system.h>
#include <core/controls/input-system.h>
#include <core/controls/glfw-bindings.h>
#include <core/game-framework/game-runtime.h>
#include <core/game-framework/abstract-game.h>
#include <core/rendering/render-frame.h>
#include <core/resources/asset-management/material-mgr.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <ext/matrix_clip_space.hpp>
#include <internals/exceptions.h>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#if defined(__linux__) && defined(CHERYL_NATIVE_X11_TESTS)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/keysym.h>
#undef None
#endif

#include <cstdlib>
#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
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

namespace {
    struct NativeTickRecord {
        CE::GFramework::UpdateKind kind;
        double delta;
        double dropped;
        double observed;
        std::vector<std::uint64_t> polls;
        std::vector<CE::Input::InputRecord> records;
        CE::Input::ButtonTickState observed_button;
    };

    struct NativeRuntimeGame : CE::GFramework::AbstractGame {
        CE::Engine::EngineContext& engine;
        NativeShaderFiles files;
        std::shared_ptr<CE::Assets::Geometry2D> geometry;
        std::shared_ptr<const CE::Assets::Material> material;
        std::vector<NativeTickRecord> ticks; // Simulation-owned until run() joins.
        mutable std::atomic<std::size_t> preparations{0};
        std::function<void()> after_update;
        std::function<void()> after_init;
        std::optional<CE::Input::ActionId> observed_action;

        explicit NativeRuntimeGame(
            CE::Engine::EngineContext& context
        )
        : engine(context) {}
        void init() override {
            geometry = fullscreen_triangle(engine.resources());
            material = material_builder(files.stages(), {1, 0, 0, 1})(engine.resources());
            if (after_init)
                after_init();
        }
        void update(
            const CE::GFramework::TickContext& tick
        ) override {
            ticks.push_back({tick.update_kind, tick.delta_seconds, tick.dropped_seconds, tick.observed_seconds(), {}, {}, {}});
            for (const auto& poll : tick.input.polls())
                ticks.back().polls.push_back(poll->poll());
            ticks.back().records.assign(tick.input.records().begin(), tick.input.records().end());
            if (observed_action)
                ticks.back().observed_button = tick.input.button(*observed_action);
            EXPECT_EQ(tick.framebuffer_size, (CE::FramebufferSize{64, 64}));
            after_update();
        }
        void prepare_render_frame(
            CE::RenderAPIs::RenderFrameWriter& frame
        ) const override {
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
    class DelayedNativeSurface final : public CE::RenderAPIs::iOpenGLContext {
        CE::RenderAPIs::GlfwOpenGLContext native_;
        bool delayed_ = false;
    public:
        std::function<void()> after_first_swap;
        std::size_t presentations = 0;
        explicit DelayedNativeSurface(
            CE::Window& window
        )
        : native_(window, 0) {}
        void make_current() override { native_.make_current(); }
        void release_current() override { native_.release_current(); }
        bool is_current() const override { return native_.is_current(); }
        ProcAddress proc_address(
            const char* name
        ) const override {
            return native_.proc_address(name);
        }
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

    std::unique_ptr<CE::Engine::EngineContext> delayed_native_context(
        DelayedNativeSurface*& observed,
        std::unique_ptr<CE::Input::iInputSystem> input = std::make_unique<CE::Input::InputSystem>()
    ) {
        auto display = std::make_unique<CE::DisplaySystem>();
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
        auto* window =
            display->create_window(display->primary_monitor(), CE::Enum::window_mode::NORMAL, 64, 64, "Cheryl delayed native presentation");
        display->activate_window(*window);
        auto surface = std::make_unique<DelayedNativeSurface>(*window);
        observed = surface.get();
        auto renderer = std::make_unique<CE::RenderAPIs::OpenGLRenderer>(*surface);
        auto resources = std::make_unique<CE::Assets::OpenGLResourceProvider>(*renderer);
        return std::make_unique<CE::Engine::EngineContext>(std::move(display), std::move(surface), std::move(renderer),
            std::move(resources), std::move(input));
    }

    // Forward complete native State snapshots; no synthetic input samples.
    // This acceptance scope needs only State, so Events/Text remain on native_.
    class ObservedNativeInput final : public CE::Input::iInputSystem {
        CE::Input::InputSystem native_;
        std::mutex mutex_;
        std::condition_variable progress_;
        std::uint64_t sequence_ = 0;
    public:
        std::function<void()> before_poll;
        std::function<void()> after_poll;
        bool deinitialized = false;
        void initialize(
            CE::iWindow& window
        ) override {
            native_.initialize(window);
        }
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
        CE::Input::InputBindings& bindings() override { return native_.bindings(); }
        std::shared_ptr<const CE::Input::ActionSnapshot> action_snapshot() override { return native_.action_snapshot(); }
        std::shared_ptr<const CE::Input::PollSnapshot> poll_snapshot() override { return native_.poll_snapshot(); }
        CE::Input::DeviceId keyboard_id() const override { return native_.keyboard_id(); }
        CE::Input::DeviceId mouse_id() const override { return native_.mouse_id(); }
        CE::Input::DeviceId gamepad_id() const override { return native_.gamepad_id(); }
        CE::Input::InputSystem& native() { return native_; }
        bool wait_for_sequence(
            std::uint64_t target
        ) {
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
        std::shared_ptr<CE::Assets::Image> image;
        std::weak_ptr<CE::Assets::Geometry2D> geometry_owner;
        std::weak_ptr<const CE::Assets::Material> material_owner;
        std::weak_ptr<CE::Assets::Image> image_owner;
        std::optional<CE::Engine::WorkerGroup> jobs;
        std::future<int> completion;
        std::promise<void> release_cpu;
        std::size_t quiesces = 0;
        std::size_t deinits = 0;

        NativeFailureGame(
            CE::Engine::EngineContext& context,
            DelayedNativeSurface& presentation,
            const NativeRuntimeFault requested
        )
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
            const auto& pipeline = dynamic_cast<const CE::Assets::GLSLPipeline&>(*material->definition().pipeline);
            pipeline.bind_parameters(material->resolve({}, {}, {}, {}));
            GLint program = 0;
            glGetIntegerv(GL_CURRENT_PROGRAM, &program);
            observed->program = static_cast<GLuint>(program);
            image = engine.resources().create_image(CE::Assets::DecodedImage{{1, 1}, {255, 255, 255, 255}});
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
                auto uploaded =
                    platform.submit([observed, owned_cpu_data = std::move(owned_cpu_data), geometry = std::move(geometry),
                                        material = std::move(material), image = std::move(image)](CE::Engine::EngineContext& context) {
                        EXPECT_EQ(std::this_thread::get_id(), observed->platform_owner);
                        EXPECT_EQ(glIsProgram(observed->program), GL_TRUE);
                        EXPECT_EQ(glIsVertexArray(observed->vao), GL_TRUE);
                        EXPECT_EQ(glIsBuffer(observed->buffer), GL_TRUE);
                        EXPECT_EQ(glIsTexture(observed->texture), GL_TRUE);
                        auto shutdown_image = context.resources().create_image(CE::Assets::DecodedImage{{1, 1}, {0, 255, 0, 255}});
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
        void prepare_render_frame(
            CE::RenderAPIs::RenderFrameWriter& frame
        ) const override {
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

TEST(
    native_opengl,
    runtime_failure_settles_native_dependencies_and_recovers_cleanup_context_in_both_modes
) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    using namespace CE::GFramework;
    std::size_t exercised = 0;
    for (const auto mode : {RunMode::Sequential, RunMode::Concurrent}) {
        for (const auto fault : {NativeRuntimeFault::Initialization, NativeRuntimeFault::PartialFrame, NativeRuntimeFault::Presentation}) {
            SCOPED_TRACE(::testing::Message() << "mode=" << static_cast<int>(mode) << " fault=" << static_cast<int>(fault));
            DelayedNativeSurface* surface = nullptr;
            auto native_input = std::make_unique<ObservedNativeInput>();
            auto* input = native_input.get();
            auto engine = delayed_native_context(surface, std::move(native_input));
            NativeFailureGame game(*engine, *surface, fault);
            GameRuntime runtime(*engine, game, mode);
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
            EXPECT_THROW(static_cast<void>(engine->platform_dispatcher().submit([](CE::Engine::EngineContext&) {})),
                CE::Exceptions::failed_operation);
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

TEST(
    native_opengl,
    real_runtime_recovers_from_slow_updates_in_both_modes
) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    using namespace CE::GFramework;
    for (const auto mode : {RunMode::Sequential, RunMode::Concurrent}) {
        for (int policy = 0; policy < 4; ++policy) {
            SCOPED_TRACE(::testing::Message() << "run mode=" << static_cast<int>(mode) << " policy=" << policy);
            auto engine = CE::Engine::make_glfw_opengl_context(small_window());
            NativeRuntimeGame game(*engine);
            SimulationTimingOptions timing;
            timing.variable_interval = std::chrono::milliseconds{5};
            timing.fixed_step = std::chrono::milliseconds{10};
            timing.max_fixed_updates = 2;
            timing.recovery_cap = std::chrono::milliseconds{15};
            if (policy != 0)
                timing.mode = SimulationMode::Fixed;
            if (policy >= 2) {
                timing.recovery = LagRecovery::VariableCatchUp;
                timing.fixed_updates_before_recovery = policy == 2 ? 1 : 0;
            }
            GameRuntime runtime(*engine, game, mode, {}, timing);
            game.after_update = [&] {
                if (game.ticks.size() == 1) {
                    // This is deliberately slow work, not a sleep used to prove
                    // thread ordering. The next real-clock batch must recover it.
                    const auto deadline = SimulationClock::now() + std::chrono::milliseconds{80};
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
                    EXPECT_EQ(tick.kind, UpdateKind::Variable);
                    EXPECT_EQ(tick.dropped, 0);
                } else if (tick.kind == UpdateKind::Fixed) {
                    EXPECT_DOUBLE_EQ(tick.delta, 0.01);
                } else {
                    EXPECT_GE(policy, 2);
                    EXPECT_EQ(tick.kind, UpdateKind::VariableCatchUp);
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

TEST(
    native_opengl,
    real_runtime_keeps_simulation_advancing_during_slow_presentation
) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    using namespace CE::GFramework;
    for (const auto mode : {RunMode::Sequential, RunMode::Concurrent}) {
        SCOPED_TRACE(static_cast<int>(mode));
        DelayedNativeSurface* surface = nullptr;
        auto engine = delayed_native_context(surface);
        NativeRuntimeGame game(*engine);
        SimulationTimingOptions timing;
        timing.variable_interval = std::chrono::milliseconds{5};
        GameRuntime runtime(*engine, game, mode, {}, timing);
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
            if (mode == RunMode::Sequential) {
                std::this_thread::sleep_until(SimulationClock::now() + std::chrono::milliseconds{80});
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
        if (mode == RunMode::Concurrent) {
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

TEST(
    native_opengl,
    full_native_poll_batches_pause_input_but_keep_presenting_and_dispatching
) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real GLFW display to run native acceptance";
    using namespace CE::GFramework;
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
        SimulationTimingOptions timing;
        timing.variable_interval = std::chrono::milliseconds{5};
        GameRuntime runtime(*engine, game, RunMode::Concurrent, polling, timing);
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
                    auto request = engine->platform_dispatcher().submit([&, drain](CE::Engine::EngineContext&) {
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
    void send_x11_key(
        CE::Window& window,
        const KeySym symbol,
        const int type,
        const unsigned state,
        Time& time
    ) {
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

TEST(
    native_opengl,
    x11_ordered_events_text_and_focus_survive_full_backlog_and_recovery
) {
    if (!native_checks_requested())
        GTEST_SKIP() << "Set CHERYL_NATIVE_GL_TESTS=1 with a real X11 GLFW display to run native acceptance";
    using namespace CE::GFramework;
    using namespace CE::Input;
    for (const auto mode : {RunMode::Sequential, RunMode::Concurrent}) {
        for (const auto recovery : {LagRecovery::DropExcessLag, LagRecovery::VariableCatchUp}) {
            SCOPED_TRACE(::testing::Message() << "mode=" << static_cast<int>(mode) << " recovery=" << static_cast<int>(recovery));
            auto input = std::make_unique<ObservedNativeInput>();
            auto* observed = input.get();
            DelayedNativeSurface* surface = nullptr;
            auto engine = delayed_native_context(surface, std::move(input));
            if (glfwGetPlatform() != GLFW_PLATFORM_X11)
                GTEST_SKIP() << "This native record case requires GLFW's X11 backend";
            surface->after_first_swap = [] {};
            auto& window = dynamic_cast<CE::Window&>(engine->window());
            NativeRuntimeGame game(*engine);
            constexpr ActionId typing_action{91};
            game.observed_action = typing_action;
            PollingOptions polling;
            polling.policy = PollingPolicy::Finite;
            polling.capacity = 3;
            SimulationTimingOptions timing;
            timing.mode = SimulationMode::Fixed;
            timing.fixed_step = std::chrono::milliseconds{10};
            timing.max_fixed_updates = 2;
            timing.recovery = recovery;
            timing.fixed_updates_before_recovery = 1;
            timing.recovery_cap = std::chrono::milliseconds{20};
            GameRuntime runtime(*engine, game, mode, polling, timing);
            CaptureLease old_events, old_text, new_events, new_text;
            FocusLease old_focus, new_focus;
            std::uint64_t old_epoch = 0;
            Time event_time = 100;
            std::promise<void> full_verified;
            auto verified = full_verified.get_future();
            std::future<void> gate;
            game.after_init = [&] {
                auto& native = observed->native();
                for (const auto key : {GLFW_KEY_A, GLFW_KEY_B, GLFW_KEY_C})
                    (void)native.bindings().bind_button({native.keyboard_id(), gainput_key(key)}, typing_action);
                old_events = native.capture(InputMode::Events);
                old_text = native.capture(InputMode::Text);
                old_focus = native.routing().focus(7);
                old_epoch = old_focus.epoch();
                if (mode == RunMode::Concurrent)
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
                new_focus = native.routing().focus(42, KeyboardRouting::PassThrough);
                old_focus.reset(); // Cannot clear the replacement owner.
                old_events.reset();
                old_text.reset();
                new_events = native.capture(InputMode::Events);
                new_text = native.capture(InputMode::Text);
                // The concurrent gate keeps this batch full. Server events wait
                // for resumed polling. Sequential mode tests the same native
                // record/focus transition between polls on the shared owner.
                send_x11_key(window, XK_c, KeyPress, 0, event_time);
                send_x11_key(window, XK_c, KeyRelease, 0, event_time);
                if (mode == RunMode::Concurrent)
                    auto first = engine->platform_dispatcher().submit([&](CE::Engine::EngineContext&) {
                        EXPECT_EQ(observed->sequence(), 3u);
                        auto second = engine->platform_dispatcher().submit([&](CE::Engine::EngineContext&) {
                            EXPECT_EQ(observed->sequence(), 3u);
                            full_verified.set_value();
                        });
                    });
            };
            game.after_update = [&] {
                if (game.ticks.size() == 1)
                    std::this_thread::sleep_until(SimulationClock::now() + std::chrono::milliseconds{120});
                if (game.ticks.size() == 16)
                    runtime.stop();
            };
            ASSERT_NO_THROW(runtime.run());
            if (gate.valid())
                ASSERT_NO_THROW(gate.get());
            ASSERT_EQ(game.ticks.size(), 16u);
            if (mode == RunMode::Concurrent) {
                ASSERT_EQ(game.ticks.front().polls.size(), 3u);
                ASSERT_EQ(game.ticks.front().records.size(), 8u);
            }
            std::vector<InputRecord> earlier;
            for (const auto& tick : game.ticks)
                earlier.insert(earlier.end(), tick.records.begin(), tick.records.end());
            ASSERT_EQ(earlier.size(), 11u);
            const std::array<ButtonPhase, 5> phases{
                ButtonPhase::Press, ButtonPhase::Repeat, ButtonPhase::Release, ButtonPhase::Press, ButtonPhase::Release};
            const std::array<std::size_t, 5> buttons{0, 2, 4, 5, 7};
            for (std::size_t i = 0; i < buttons.size(); ++i) {
                ASSERT_TRUE(std::holds_alternative<ButtonEvent>(earlier[buttons[i]].data));
                EXPECT_EQ(std::get<ButtonEvent>(earlier[buttons[i]].data).phase, phases[i]);
                EXPECT_EQ(std::get<ButtonEvent>(earlier[buttons[i]].data).native_code, i < 3 ? GLFW_KEY_A : GLFW_KEY_B);
                EXPECT_EQ(has_modifier(std::get<ButtonEvent>(earlier[buttons[i]].data).modifiers, Modifiers::Shift), i < 3);
            }
            EXPECT_EQ(std::get<TextEvent>(earlier[1].data).codepoint, U'A');
            EXPECT_EQ(std::get<TextEvent>(earlier[3].data).codepoint, U'A');
            EXPECT_EQ(std::get<TextEvent>(earlier[6].data).codepoint, U'b');
            ASSERT_TRUE(std::holds_alternative<ButtonEvent>(earlier[8].data));
            EXPECT_EQ(std::get<ButtonEvent>(earlier[8].data).phase, ButtonPhase::Press);
            EXPECT_EQ(std::get<ButtonEvent>(earlier[8].data).native_code, GLFW_KEY_C);
            ASSERT_TRUE(std::holds_alternative<TextEvent>(earlier[9].data));
            EXPECT_EQ(std::get<TextEvent>(earlier[9].data).codepoint, U'c');
            ASSERT_TRUE(std::holds_alternative<ButtonEvent>(earlier[10].data));
            EXPECT_EQ(std::get<ButtonEvent>(earlier[10].data).phase, ButtonPhase::Release);
            EXPECT_EQ(std::get<ButtonEvent>(earlier[10].data).native_code, GLFW_KEY_C);
            std::size_t delivered = 0, c_presses = 0, c_releases = 0;
            std::uint64_t sequence = 0;
            bool saw_drop = false, saw_recovery = false;
            InputClock::time_point previous_observation = InputClock::time_point::min();
            for (const auto& tick : game.ticks) {
                saw_drop |= tick.dropped > 0;
                saw_recovery |= tick.kind == UpdateKind::VariableCatchUp;
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
                        if (const auto* text = std::get_if<TextEvent>(&record.data))
                            EXPECT_EQ(text->codepoint, U'c');
                    }
                    ++delivered;
                }
            }
            EXPECT_EQ(delivered, 11u); // Eight old-focus records, then one three-record C tap.
            EXPECT_EQ(c_presses, 1u);
            EXPECT_EQ(c_releases, 1u);
            EXPECT_TRUE(saw_drop);
            EXPECT_EQ(saw_recovery, recovery == LagRecovery::VariableCatchUp);
            const auto prefix = mode == RunMode::Sequential ? "sequential_" : "concurrent_";
            RecordProperty(std::string(prefix) + (recovery == LagRecovery::DropExcessLag ? "drop_records" : "catchup_records"),
                static_cast<int>(delivered));
        }
    }
}
#endif

#endif
