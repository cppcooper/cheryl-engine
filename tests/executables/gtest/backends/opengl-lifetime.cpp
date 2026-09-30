#include <backends/opengl/resource-lifetime.h>
#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <atomic>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
    using CE::Exceptions::failed_operation;
    using CE::RenderAPIs::GLResourceKind;
    using CE::RenderAPIs::OpenGLHandle;
    using CE::RenderAPIs::OpenGLResourceLifetime;
}

TEST(opengl_lifetime, the_owner_thread_also_needs_the_correct_current_context) {
    bool current = false;
    int queries = 0;
    OpenGLResourceLifetime lifetime(std::this_thread::get_id(), [&] {
        ++queries;
        return current;
    });
    EXPECT_NO_THROW(lifetime.require_owner());
    EXPECT_EQ(queries, 0);
    EXPECT_THROW(lifetime.require_current(), failed_operation);
    EXPECT_THROW(static_cast<void>(lifetime.track(GLResourceKind::Texture, 1)), failed_operation);
    current = true;
    EXPECT_NO_THROW(lifetime.require_current());
    // No handles were registered, so this source test needs no actual OpenGL calls.
    EXPECT_NO_THROW(lifetime.shutdown());
}

TEST(opengl_lifetime, foreign_threads_and_closed_lifetimes_never_query_a_borrowed_context) {
    std::atomic<int> queries = 0;
    bool context_alive = true;
    auto lifetime = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [&] {
        ++queries;
        if (!context_alive)
            throw std::logic_error("queried a destroyed context");
        return true;
    });
    auto worker = std::async(std::launch::async, [lifetime] {
        try {
            lifetime->require_current();
            return false;
        }
        catch (const failed_operation&) {
            return true;
        }
    });
    EXPECT_TRUE(worker.get());
    EXPECT_EQ(queries.load(), 0);
    lifetime->shutdown();
    context_alive = false;
    const auto closed_queries = queries.load();
    EXPECT_THROW(lifetime->require_current(), failed_operation);
    EXPECT_THROW(static_cast<void>(lifetime->track(GLResourceKind::Buffer, 1)), failed_operation);
    EXPECT_EQ(queries.load(), closed_queries);
}

TEST(opengl_lifetime, failed_context_recovery_invalidates_retained_handles_without_gl_calls) {
    bool current = true;
    int queries = 0;
    auto lifetime = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [&] {
        ++queries;
        return current;
    });
    // A synthetic ID is safe here: abandon() and handle destruction never call OpenGL.
    auto retained = std::make_unique<OpenGLHandle>(lifetime, GLResourceKind::Program, 7);
    current = false;
    lifetime->abandon();
    const auto closed_queries = queries;
    EXPECT_THROW((void)retained->id(), failed_operation);
    retained.reset();
    EXPECT_EQ(queries, closed_queries);
}
