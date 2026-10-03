#include "debug-output.h"
#include <internals/compile-time-logging.hpp>

namespace CE::RenderAPIs::RendererDetail {
    thread_local DebugOutput* DebugOutput::observing_ = nullptr;

    DebugOutput::Scope::Scope(DebugOutput* output) noexcept
    : previous_(observing_) {
        observing_ = output;
    }

    DebugOutput::Scope::~Scope() {
        observing_ = previous_;
    }

    void GLAD_API_PTR DebugOutput::callback(
        const GLenum source,
        const GLenum type,
        const GLuint id,
        const GLenum severity,
        GLsizei,
        const GLchar*,
        const void*
    ) noexcept {
        if (!observing_)
            return;
        auto& stats = observing_->stats_;
        if (severity == GL_DEBUG_SEVERITY_HIGH)
            ++stats.high;
        else if (severity == GL_DEBUG_SEVERITY_MEDIUM)
            ++stats.medium;
        else {
            ++stats.filtered;
            return;
        }
        stats.last_id = id;
        stats.last_source = source;
        stats.last_type = type;
    }

    void DebugOutput::start(const Diagnostics::DomainId domain) noexcept {
        stats_.domain = domain;
        stats_.supported = (GLAD_GL_VERSION_4_3 || GLAD_GL_KHR_debug) && glDebugMessageCallback && glGetPointerv && glIsEnabled &&
                           glEnable && glDisable && glGetError;
        if (!stats_.supported)
            return;
        void* previous = nullptr;
        glGetPointerv(GL_DEBUG_CALLBACK_FUNCTION, &previous);
        if (previous) {
            stats_.host_owned = true;
            return;
        }
        glGetPointerv(GL_DEBUG_CALLBACK_USER_PARAM, &previous_user_);
        previous_enabled_ = glIsEnabled(GL_DEBUG_OUTPUT);
        previous_synchronous_ = glIsEnabled(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(callback, nullptr);
        stats_.installed = true;
        glEnable(GL_DEBUG_OUTPUT);
        next_report_ = std::chrono::steady_clock::now() + std::chrono::seconds{2};
        if (const auto error = glGetError(); error != GL_NO_ERROR)
            Diagnostics::report_outcome("native_debug", domain, "enable", "native_failure", error);
    }

    void DebugOutput::stop() noexcept {
        if (!stats_.installed)
            return;
        void* current = nullptr;
        glGetPointerv(GL_DEBUG_CALLBACK_FUNCTION, &current);
        // A host replacement owns its new flags and callback; do not overwrite it.
        if (reinterpret_cast<GLDEBUGPROC>(current) == callback) {
            glDebugMessageCallback(nullptr, previous_user_);
            if (!previous_enabled_)
                glDisable(GL_DEBUG_OUTPUT);
            if (!previous_synchronous_)
                glDisable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        } else
            stats_.host_owned = true;
        stats_.installed = false;
        if (const auto error = glGetError(); error != GL_NO_ERROR)
            Diagnostics::report_outcome("native_debug", stats_.domain, "restore", "native_failure", error);
    }

    void DebugOutput::report(const bool force) noexcept {
        const auto now = std::chrono::steady_clock::now();
        if (!force && now < next_report_)
            return;
        next_report_ = now + std::chrono::seconds{2};
        if (stats_.high > reported_high_)
            CE_LOG_ERROR(
                CE::enginelog, "subsystem=native_debug domain={} operation=summary high={} new={} last_id={} source={} type={}",
                stats_.domain, stats_.high, stats_.high - reported_high_, stats_.last_id, stats_.last_source, stats_.last_type
            );
        if (stats_.medium > reported_medium_)
            CE_LOG_WARN(
                CE::enginelog, "subsystem=native_debug domain={} operation=summary medium={} new={} filtered={}", stats_.domain,
                stats_.medium, stats_.medium - reported_medium_, stats_.filtered
            );
        reported_high_ = stats_.high;
        reported_medium_ = stats_.medium;
    }
}
