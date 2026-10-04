#include <gtest/gtest.h>
#include <backends/opengl/debug-output.h>

namespace {
    class DebugNative {
        decltype(glad_glDebugMessageCallback) saved_callback_ = glad_glDebugMessageCallback;
        decltype(glad_glGetPointerv) saved_pointer_ = glad_glGetPointerv;
        decltype(glad_glIsEnabled) saved_is_enabled_ = glad_glIsEnabled;
        decltype(glad_glEnable) saved_enable_ = glad_glEnable;
        decltype(glad_glDisable) saved_disable_ = glad_glDisable;
        decltype(glad_glGetError) saved_error_ = glad_glGetError;
        int saved_core_ = GLAD_GL_VERSION_4_3;
        int saved_extension_ = GLAD_GL_KHR_debug;
        inline static DebugNative* active_;

    public:
        GLDEBUGPROC callback = nullptr;
        const void* user = nullptr;
        bool enabled = false;
        bool synchronous = false;
        int calls = 0;

        DebugNative() {
            active_ = this;
            GLAD_GL_VERSION_4_3 = 0;
            GLAD_GL_KHR_debug = 1;
            glad_glDebugMessageCallback = [](GLDEBUGPROC callback, const void* user) {
                ++active_->calls;
                active_->callback = callback;
                active_->user = user;
            };
            glad_glGetPointerv = [](GLenum parameter, void** result) {
                ++active_->calls;
                *result =
                    parameter == GL_DEBUG_CALLBACK_FUNCTION ? reinterpret_cast<void*>(active_->callback) : const_cast<void*>(active_->user);
            };
            glad_glIsEnabled = [](GLenum flag) -> GLboolean { return flag == GL_DEBUG_OUTPUT ? active_->enabled : active_->synchronous; };
            glad_glEnable = [](GLenum flag) { (flag == GL_DEBUG_OUTPUT ? active_->enabled : active_->synchronous) = true; };
            glad_glDisable = [](GLenum flag) { (flag == GL_DEBUG_OUTPUT ? active_->enabled : active_->synchronous) = false; };
            glad_glGetError = []() -> GLenum { return GL_NO_ERROR; };
        }

        ~DebugNative() {
            glad_glDebugMessageCallback = saved_callback_;
            glad_glGetPointerv = saved_pointer_;
            glad_glIsEnabled = saved_is_enabled_;
            glad_glEnable = saved_enable_;
            glad_glDisable = saved_disable_;
            glad_glGetError = saved_error_;
            GLAD_GL_VERSION_4_3 = saved_core_;
            GLAD_GL_KHR_debug = saved_extension_;
            active_ = nullptr;
        }
    };
}

TEST(opengl_debug, baseline_gate) {
    DebugNative native;
    GLAD_GL_KHR_debug = 0;
    CE::RenderAPIs::RendererDetail::DebugOutput output;
    output.start(17);
    EXPECT_FALSE(output.diagnostics().supported);
    EXPECT_EQ(native.calls, 0);
}

TEST(opengl_debug, host_callback) {
    DebugNative native;
    native.callback = [](GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, const void*) {};
    const auto previous = native.callback;
    CE::RenderAPIs::RendererDetail::DebugOutput output;
    output.start(18);
    EXPECT_TRUE(output.diagnostics().host_owned);
    EXPECT_FALSE(output.diagnostics().installed);
    output.stop();
    EXPECT_EQ(native.callback, previous);
    EXPECT_FALSE(native.enabled);
}

TEST(opengl_debug, scoped_observation) {
    DebugNative native;
    CE::RenderAPIs::RendererDetail::DebugOutput output;
    output.start(19);
    ASSERT_NE(native.callback, nullptr);
    EXPECT_EQ(native.user, nullptr);
    EXPECT_TRUE(native.synchronous);
    {
        CE::RenderAPIs::RendererDetail::DebugOutput::Scope observing(&output);
        for (int i = 0; i < 100; ++i)
            native.callback(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, 71, GL_DEBUG_SEVERITY_HIGH, 0, nullptr, nullptr);
        native.callback(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER, 72, GL_DEBUG_SEVERITY_MEDIUM, 0, nullptr, nullptr);
        native.callback(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER, 73, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, nullptr);
    }
    native.callback(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, 74, GL_DEBUG_SEVERITY_HIGH, 0, nullptr, nullptr);
    const auto stats = output.diagnostics();
    EXPECT_EQ(stats.high, 100u);
    EXPECT_EQ(stats.medium, 1u);
    EXPECT_EQ(stats.filtered, 1u);
    EXPECT_EQ(stats.last_id, 72u);
    output.stop();
    EXPECT_EQ(native.callback, nullptr);
    EXPECT_FALSE(native.enabled);
    EXPECT_FALSE(native.synchronous);
}

TEST(opengl_debug, abandoned_scope) {
    DebugNative native;
    {
        CE::RenderAPIs::RendererDetail::DebugOutput output;
        output.start(20);
        CE::RenderAPIs::RendererDetail::DebugOutput::Scope observing(&output);
        // Context recovery fails: no stop/native restoration can be attempted.
    }
    ASSERT_NE(native.callback, nullptr);
    EXPECT_EQ(native.user, nullptr);
    EXPECT_NO_THROW(native.callback(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, 75, GL_DEBUG_SEVERITY_HIGH, 0, nullptr, nullptr));
}

TEST(opengl_debug, host_replacement) {
    DebugNative native;
    CE::RenderAPIs::RendererDetail::DebugOutput output;
    output.start(21);
    native.callback = [](GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, const void*) {};
    const auto host = native.callback;
    output.stop();
    EXPECT_EQ(native.callback, host);
    EXPECT_TRUE(native.enabled);
    EXPECT_TRUE(output.diagnostics().host_owned);
}
