#include <backends/opengl/resource-lifetime.h>
#include <backends/opengl/renderer.h>
#include <backends/opengl/glslprogram.h>
#include <backends/opengl/resource-lifetime-internal.h>
#include <testing/failing-memory-resource.h>
#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <atomic>
#include <future>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {
    using CE::Exceptions::failed_operation;
    using CE::RenderAPIs::GLResourceKind;
    using CE::RenderAPIs::OpenGLHandle;
    using CE::RenderAPIs::OpenGLResourceLifetime;

    class FailingStartupContext final : public CE::RenderAPIs::iOpenGLContext {
    public:
        bool become_current = true;
        bool fail_acquisition = true;
        bool fail_lookup = false;
        bool fail_release = false;
        bool current = false;
        int acquisitions = 0;
        int releases = 0;
        mutable int lookups = 0;

        void make_current() override {
            ++acquisitions;
            current = become_current;
            if (fail_acquisition)
                throw std::runtime_error("Original context acquisition failure");
        }

        void release_current() override {
            ++releases;
            if (!current)
                throw std::runtime_error("Context was never current");
            current = false;
            if (fail_release)
                throw std::runtime_error("Later context release failure");
        }

        [[nodiscard]] bool is_current() const override { return current; }
        [[nodiscard]] ProcAddress proc_address(const char*) const override {
            ++lookups;
            if (fail_lookup)
                throw std::runtime_error("Original procedure lookup failure");
            return nullptr;
        }
        void present() override {}
    };

    /** Records native deletion calls without constructing a real GL context.
     * Restores GLAD's process-wide entry points before another scenario runs.
     */
    class DeletionRecorder final {
        decltype(glad_glDeleteTextures) textures_ = glad_glDeleteTextures;
        decltype(glad_glDeleteBuffers) buffers_ = glad_glDeleteBuffers;
        decltype(glad_glDeleteVertexArrays) arrays_ = glad_glDeleteVertexArrays;
        decltype(glad_glDeleteProgram) programs_ = glad_glDeleteProgram;
        decltype(glad_glDeleteShader) shaders_ = glad_glDeleteShader;
        decltype(glad_glDeleteSamplers) samplers_ = glad_glDeleteSamplers;
        inline static DeletionRecorder* active_ = nullptr;

    public:
        std::vector<std::pair<GLResourceKind, GLuint>> deletions;
        std::thread::id deletion_thread;

    private:
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
        static void GLAD_API_PTR samplers(const GLsizei count, const GLuint* ids) { record(GLResourceKind::Sampler, count, ids); }

    public:
        DeletionRecorder() {
            active_ = this;
            glad_glDeleteTextures = textures;
            glad_glDeleteBuffers = buffers;
            glad_glDeleteVertexArrays = arrays;
            glad_glDeleteProgram = program;
            glad_glDeleteShader = shader;
            glad_glDeleteSamplers = samplers;
        }
        ~DeletionRecorder() {
            glad_glDeleteTextures = textures_;
            glad_glDeleteBuffers = buffers_;
            glad_glDeleteVertexArrays = arrays_;
            glad_glDeleteProgram = programs_;
            glad_glDeleteShader = shaders_;
            glad_glDeleteSamplers = samplers_;
            active_ = nullptr;
        }
        DeletionRecorder(const DeletionRecorder&) = delete;
        DeletionRecorder& operator=(const DeletionRecorder&) = delete;
    };
}

TEST(opengl_renderer, context_acquisition_failure) {
    for (const bool become_current : {false, true}) {
        for (const bool fail_release : {false, true}) {
            SCOPED_TRACE(become_current ? "partially current" : "never current");
            SCOPED_TRACE(fail_release ? "release throws" : "release succeeds when current");
            FailingStartupContext context;
            context.become_current = become_current;
            context.fail_release = fail_release;
            {
                CE::RenderAPIs::OpenGLRenderer renderer(context);
                try {
                    renderer.initialize();
                    FAIL() << "Context acquisition must fail";
                } catch (const std::runtime_error& error) {
                    EXPECT_EQ(std::string_view(error.what()), "Original context acquisition failure");
                }
                EXPECT_EQ(context.acquisitions, 1);
                EXPECT_EQ(context.releases, 1);
                EXPECT_FALSE(context.current);
                EXPECT_EQ(context.lookups, 0);
                EXPECT_THROW((void)renderer.resources(), failed_operation);
                EXPECT_NO_THROW(renderer.deinitialize());
                EXPECT_EQ(context.releases, 1);
            }
            EXPECT_EQ(context.releases, 1);
        }
    }
}

TEST(opengl_lifetime, owner_context_validation) {
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

TEST(opengl_lifetime, registry_growth_failure) {
    DeletionRecorder native;
    auto memory = std::make_shared<CE::Testing::FailingMemoryResource>();
    auto lifetime = CE::RenderAPIs::ResourceDetail::LifetimeAccess::create(std::this_thread::get_id(), [] { return true; }, memory);
    {
        OpenGLHandle live(lifetime, GLResourceKind::Buffer, 101);
        { OpenGLHandle pending(lifetime, GLResourceKind::Texture, 102); }
        const auto capacity = CE::RenderAPIs::ResourceDetail::LifetimeAccess::capacity(*lifetime);
        std::vector<OpenGLHandle> fill;
        while (2 + fill.size() < capacity)
            fill.emplace_back(lifetime, GLResourceKind::Buffer, static_cast<GLuint>(1000 + fill.size()));
        memory->reject_next();
        EXPECT_THROW((void)OpenGLHandle(lifetime, GLResourceKind::ShaderStage, 103), std::bad_alloc);
        EXPECT_EQ(memory->rejected.load(), 1u);
        EXPECT_TRUE(native.deletions.empty());
        EXPECT_EQ(live.id(), 101u);
        lifetime->discard_untracked(GLResourceKind::ShaderStage, 103);
        lifetime->collect();
        ASSERT_EQ(native.deletions.size(), 2u);
        EXPECT_EQ(native.deletions[0], (std::pair{GLResourceKind::ShaderStage, GLuint{103}}));
        EXPECT_EQ(native.deletions[1], (std::pair{GLResourceKind::Texture, GLuint{102}}));
        const auto requests = memory->requests.load();
        memory->reject_next();
        OpenGLHandle reused(lifetime, GLResourceKind::ShaderStage, 104);
        EXPECT_EQ(memory->requests.load(), requests);
        lifetime->shutdown();
        EXPECT_EQ(memory->requests.load(), requests); // Retirement/collection/sweep do not allocate entries.
        EXPECT_THROW((void)live.id(), failed_operation);
        EXPECT_THROW((void)reused.id(), failed_operation);
        EXPECT_EQ(native.deletions.size(), capacity + 2);
    }
    const auto deleted = native.deletions.size();
    EXPECT_THROW(lifetime->collect(), failed_operation); // Closed lifetime rejects before native deletion.
    EXPECT_EQ(native.deletions.size(), deleted);
}

TEST(opengl_lifetime, retained_allocator_lifetime) {
    DeletionRecorder native;
    auto memory = std::make_shared<CE::Testing::FailingMemoryResource>();
    std::weak_ptr<CE::Testing::FailingMemoryResource> borrowed_memory = memory;
    auto lifetime = CE::RenderAPIs::ResourceDetail::LifetimeAccess::create(std::this_thread::get_id(), [] { return true; }, memory);
    auto retained = std::make_unique<OpenGLHandle>(lifetime, GLResourceKind::Program, 105);
    memory.reset();
    lifetime->abandon();
    const auto stats = lifetime->diagnostics();
    EXPECT_EQ(stats.tracked, 1u);
    EXPECT_EQ(stats.abandoned, 1u);
    EXPECT_EQ(stats.live, 0u);
    EXPECT_FALSE(stats.active);
    lifetime.reset();
    EXPECT_FALSE(borrowed_memory.expired());
    retained.reset();
    EXPECT_TRUE(borrowed_memory.expired());
    EXPECT_TRUE(native.deletions.empty());
}

TEST(opengl_lifetime, borrowed_context_guards) {
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

TEST(opengl_lifetime, failed_context_recovery) {
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

TEST(opengl_lifetime, worker_release_retirement) {
    for (const auto kind : {GLResourceKind::Texture, GLResourceKind::Sampler}) {
        SCOPED_TRACE(static_cast<int>(kind));
        DeletionRecorder native;
        auto lifetime = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [] { return true; });
        auto handle = std::make_unique<OpenGLHandle>(lifetime, kind, 41);
        auto worker = std::async(std::launch::async, [handle = std::move(handle)]() mutable { handle.reset(); });
        worker.get();
        EXPECT_TRUE(native.deletions.empty());
        EXPECT_EQ(lifetime->diagnostics().pending, 1u);
        lifetime->collect();
        ASSERT_EQ(native.deletions.size(), 1u);
        EXPECT_EQ(native.deletions.front(), (std::pair{kind, GLuint{41}}));
        EXPECT_EQ(native.deletion_thread, std::this_thread::get_id());
        EXPECT_EQ(lifetime->diagnostics().pending, 0u);
        EXPECT_EQ(lifetime->diagnostics().deleted, 1u);
        lifetime->collect();
        lifetime->shutdown();
        EXPECT_EQ(native.deletions.size(), 1u);
    }
}

TEST(opengl_lifetime, retained_shutdown) {
    DeletionRecorder native;
    auto lifetime = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [] { return true; });
    auto handle = std::make_unique<OpenGLHandle>(lifetime, GLResourceKind::Buffer, 9);
    lifetime->shutdown();
    ASSERT_EQ(native.deletions.size(), 1u);
    EXPECT_THROW((void)handle->id(), failed_operation);
    handle.reset();
    EXPECT_EQ(native.deletions.size(), 1u);
}

TEST(opengl_lifetime, handle_move_and_slot_reuse) {
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

TEST(opengl_lifetime, untracked_cleanup_context) {
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

TEST(opengl_lifetime, foreign_context_maintenance) {
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

TEST(opengl_lifetime, shutdown_context_recovery) {
    DeletionRecorder native;
    bool current = true;
    int queries = 0;
    auto lifetime = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [&] {
        ++queries;
        return current;
    });
    { OpenGLHandle pending(lifetime, GLResourceKind::Texture, 51); }
    auto retained = std::make_unique<OpenGLHandle>(lifetime, GLResourceKind::Buffer, 52);
    current = false;
    EXPECT_THROW(lifetime->shutdown(), failed_operation);
    EXPECT_TRUE(native.deletions.empty());

    current = true; // Recovery may collect pending work without losing live handles.
    EXPECT_EQ(retained->id(), 52u);
    lifetime->collect();
    ASSERT_EQ(native.deletions.size(), 1u);
    EXPECT_EQ(native.deletions[0], (std::pair{GLResourceKind::Texture, GLuint{51}}));
    lifetime->shutdown();
    ASSERT_EQ(native.deletions.size(), 2u);
    EXPECT_EQ(native.deletions[1], (std::pair{GLResourceKind::Buffer, GLuint{52}}));
    EXPECT_EQ(native.deletion_thread, std::this_thread::get_id());
    const auto closed_queries = queries;
    current = false;
    auto worker = std::async(std::launch::async, [retained = std::move(retained)]() mutable { retained.reset(); });
    worker.get();
    EXPECT_EQ(queries, closed_queries);
    EXPECT_EQ(native.deletions.size(), 2u);
}

TEST(opengl_lifetime, failed_shutdown_abandonment) {
    DeletionRecorder native;
    bool current = true;
    bool context_alive = true;
    int queries = 0;
    auto lifetime = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [&] {
        ++queries;
        if (!context_alive)
            throw std::logic_error("queried a destroyed context");
        return current;
    });
    const std::vector<GLResourceKind> kinds{
        GLResourceKind::Texture, GLResourceKind::Buffer, GLResourceKind::VertexArray, GLResourceKind::Program,
        GLResourceKind::ShaderStage, GLResourceKind::Sampler};
    std::vector<std::unique_ptr<OpenGLHandle>> handles;
    GLuint id = 60;
    for (const auto kind : kinds)
        handles.push_back(std::make_unique<OpenGLHandle>(lifetime, kind, id++));
    handles.front().reset(); // One pending retirement and the remaining handles retained.
    current = false;
    EXPECT_THROW(lifetime->shutdown(), failed_operation);
    lifetime->abandon(); // Context destruction owns native cleanup when recovery fails.
    context_alive = false;
    const auto closed_queries = queries;
    for (const auto& handle : handles)
        if (handle)
            EXPECT_THROW((void)handle->id(), failed_operation);
    EXPECT_THROW(lifetime->collect(), failed_operation);
    auto worker = std::async(std::launch::async, [handles = std::move(handles)]() mutable { handles.clear(); });
    worker.get();
    EXPECT_EQ(queries, closed_queries);
    EXPECT_TRUE(native.deletions.empty());
}

TEST(opengl_lifetime, invalid_program_adoption) {
    DeletionRecorder native;
    auto lifetime = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [] { return true; });
    OpenGLHandle texture(lifetime, GLResourceKind::Texture, 29);
    EXPECT_THROW((void)CE::Assets::GLSLProgram(std::move(texture)), CE::Exceptions::invalid_args);
    EXPECT_TRUE(native.deletions.empty());
    lifetime->collect();
    ASSERT_EQ(native.deletions.size(), 1u);
    EXPECT_EQ(native.deletions.front(), (std::pair{GLResourceKind::Texture, GLuint{29}}));
    lifetime->shutdown();
    EXPECT_EQ(native.deletions.size(), 1u);
}

TEST(opengl_lifetime, invalid_registrations) {
    DeletionRecorder native;
    OpenGLResourceLifetime lifetime(std::this_thread::get_id(), [] { return true; });
    EXPECT_THROW((void)lifetime.track(static_cast<GLResourceKind>(-1), 31), CE::Exceptions::invalid_args);
    EXPECT_THROW((void)lifetime.track(GLResourceKind::Program, 0), failed_operation);
    lifetime.shutdown();
    EXPECT_TRUE(native.deletions.empty());
}

TEST(opengl_renderer, procedure_lookup_failure) {
    // The generated loader requests glGetString first and returns immediately
    // on null. Restore that process-wide entry even if an assertion fails.
    struct RestoreProc {
        decltype(glad_glGetString) original = glad_glGetString;
        ~RestoreProc() { glad_glGetString = original; }
    } restore;
    FailingStartupContext context;
    context.fail_acquisition = false;
    context.fail_lookup = true;
    context.fail_release = true;
    CE::RenderAPIs::OpenGLRenderer renderer(context);
    ::testing::internal::CaptureStderr();
    try {
        renderer.initialize();
        ADD_FAILURE() << "Procedure lookup must fail";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "Original procedure lookup failure");
    } catch (...) {
        ADD_FAILURE() << "Unexpected procedure lookup exception type";
    }
    const auto diagnostic = ::testing::internal::GetCapturedStderr();
    EXPECT_NE(diagnostic.find("OpenGL initialization context release: Later context release failure"), std::string::npos);
    EXPECT_EQ(context.lookups, 1);
    EXPECT_EQ(context.releases, 1);
    EXPECT_FALSE(context.current);
    EXPECT_NO_THROW(renderer.deinitialize());
}
