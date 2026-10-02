#ifndef CHERYL_SANDBOX_BUILD

#include <gtest/gtest.h>
#include <backends/opengl/glfw-backend.h>
#include <backends/opengl/renderer.h>
#include <backends/opengl/pipeline.h>
#include <assets/resources/resource-provider.h>
#include <backends/opengl/resource-provider.h>
#include <core/display/window.h>
#include <core/rendering/render-frame.h>
#include <core/resources/asset-management/material-mgr.h>
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

#endif
