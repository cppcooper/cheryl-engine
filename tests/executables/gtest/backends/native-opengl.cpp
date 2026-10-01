#ifndef CHERYL_SANDBOX_BUILD

#include <gtest/gtest.h>
#include <backends/opengl/glfw-backend.h>
#include <backends/opengl/renderer.h>
#include <assets/resources/resource-provider.h>
#include <core/display/window.h>
#include <internals/exceptions.h>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>

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

#endif
