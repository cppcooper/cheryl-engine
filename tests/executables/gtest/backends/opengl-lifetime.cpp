#include <backends/opengl/resource-lifetime.h>
#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <atomic>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {
    using CE::Exceptions::failed_operation;
    using CE::RenderAPIs::GLResourceKind;
    using CE::RenderAPIs::OpenGLHandle;
    using CE::RenderAPIs::OpenGLResourceLifetime;

    /** Records native deletion calls without constructing a real GL context.
     * Restores GLAD's process-wide entry points before another scenario runs.
     */
    class DeletionRecorder final {
        decltype(glad_glDeleteTextures) textures_ = glad_glDeleteTextures;
        decltype(glad_glDeleteBuffers) buffers_ = glad_glDeleteBuffers;
        decltype(glad_glDeleteVertexArrays) arrays_ = glad_glDeleteVertexArrays;
        decltype(glad_glDeleteProgram) programs_ = glad_glDeleteProgram;
        decltype(glad_glDeleteShader) shaders_ = glad_glDeleteShader;
        inline static DeletionRecorder* active_ = nullptr;

        static void record(const GLResourceKind kind, const GLsizei count, const GLuint* ids) {
            for (GLsizei i = 0; i < count; ++i)
                active_->deletions.emplace_back(kind, ids[i]);
            active_->deletion_thread = std::this_thread::get_id();
        }
        static void GLAD_API_PTR textures(const GLsizei count, const GLuint* ids) { record(GLResourceKind::Texture, count, ids); }
        static void GLAD_API_PTR buffers(const GLsizei count, const GLuint* ids) { record(GLResourceKind::Buffer, count, ids); }
        static void GLAD_API_PTR arrays(const GLsizei count, const GLuint* ids) { record(GLResourceKind::VertexArray, count, ids); }
        static void GLAD_API_PTR program(const GLuint id) { record(GLResourceKind::Program, 1, &id); }
        static void GLAD_API_PTR shader(const GLuint id) { record(GLResourceKind::ShaderStage, 1, &id); }

    public:
        std::vector<std::pair<GLResourceKind, GLuint>> deletions;
        std::thread::id deletion_thread;

        DeletionRecorder() {
            active_ = this;
            glad_glDeleteTextures = textures;
            glad_glDeleteBuffers = buffers;
            glad_glDeleteVertexArrays = arrays;
            glad_glDeleteProgram = program;
            glad_glDeleteShader = shader;
        }
        ~DeletionRecorder() {
            glad_glDeleteTextures = textures_;
            glad_glDeleteBuffers = buffers_;
            glad_glDeleteVertexArrays = arrays_;
            glad_glDeleteProgram = programs_;
            glad_glDeleteShader = shaders_;
            active_ = nullptr;
        }
        DeletionRecorder(const DeletionRecorder&) = delete;
        DeletionRecorder& operator=(const DeletionRecorder&) = delete;
    };
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
        } catch (const failed_operation&) {
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

TEST(opengl_lifetime, last_owner_release_on_a_worker_is_deleted_once_by_owner_maintenance) {
    DeletionRecorder native;
    auto lifetime = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [] { return true; });
    auto handle = std::make_unique<OpenGLHandle>(lifetime, GLResourceKind::Texture, 41);
    auto worker = std::async(std::launch::async, [handle = std::move(handle)]() mutable { handle.reset(); });
    worker.get();
    EXPECT_TRUE(native.deletions.empty());
    lifetime->collect();
    ASSERT_EQ(native.deletions.size(), 1u);
    EXPECT_EQ(native.deletions.front(), (std::pair{GLResourceKind::Texture, GLuint{41}}));
    EXPECT_EQ(native.deletion_thread, std::this_thread::get_id());
    lifetime->collect();
    lifetime->shutdown();
    EXPECT_EQ(native.deletions.size(), 1u);
}

TEST(opengl_lifetime, shutdown_deletes_a_retained_resource_and_later_release_does_not_delete_again) {
    DeletionRecorder native;
    auto lifetime = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [] { return true; });
    auto handle = std::make_unique<OpenGLHandle>(lifetime, GLResourceKind::Buffer, 9);
    lifetime->shutdown();
    ASSERT_EQ(native.deletions.size(), 1u);
    EXPECT_THROW((void)handle->id(), failed_operation);
    handle.reset();
    EXPECT_EQ(native.deletions.size(), 1u);
}

TEST(opengl_lifetime, moved_handles_and_reused_registration_slots_keep_distinct_ownership) {
    DeletionRecorder native;
    auto lifetime = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [] { return true; });
    {
        OpenGLHandle source(lifetime, GLResourceKind::Program, 11);
        OpenGLHandle target(std::move(source));
        EXPECT_THROW((void)source.id(), failed_operation);
        EXPECT_EQ(target.id(), 11u);
    }
    lifetime->collect();
    {
        OpenGLHandle next(lifetime, GLResourceKind::VertexArray, 12);
        EXPECT_EQ(next.id(), 12u);
    }
    lifetime->collect();
    lifetime->shutdown();
    ASSERT_EQ(native.deletions.size(), 2u);
    EXPECT_EQ(native.deletions[0], (std::pair{GLResourceKind::Program, GLuint{11}}));
    EXPECT_EQ(native.deletions[1], (std::pair{GLResourceKind::VertexArray, GLuint{12}}));
}

TEST(opengl_lifetime, untracked_failure_cleanup_requires_the_owner_and_its_actual_context) {
    DeletionRecorder native;
    bool current = false;
    auto lifetime = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [&] { return current; });
    lifetime->discard_untracked(GLResourceKind::Program, 17);
    EXPECT_TRUE(native.deletions.empty());
    current = true;
    auto worker = std::async(std::launch::async, [lifetime] { lifetime->discard_untracked(GLResourceKind::Texture, 18); });
    worker.get();
    EXPECT_TRUE(native.deletions.empty());
    lifetime->discard_untracked(GLResourceKind::ShaderStage, 19);
    ASSERT_EQ(native.deletions.size(), 1u);
    EXPECT_EQ(native.deletions[0], (std::pair{GLResourceKind::ShaderStage, GLuint{19}}));
    lifetime->shutdown();
    lifetime->discard_untracked(GLResourceKind::Program, 20);
    EXPECT_EQ(native.deletions.size(), 1u);
}

TEST(opengl_lifetime, maintenance_with_a_different_current_context_does_not_delete_pending_ids) {
    DeletionRecorder native;
    bool current = true;
    auto lifetime = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [&] { return current; });
    { OpenGLHandle retired(lifetime, GLResourceKind::Texture, 23); }
    current = false;
    EXPECT_THROW(lifetime->collect(), failed_operation);
    EXPECT_TRUE(native.deletions.empty());
    current = true;
    lifetime->collect();
    ASSERT_EQ(native.deletions.size(), 1u);
    lifetime->shutdown();
}
