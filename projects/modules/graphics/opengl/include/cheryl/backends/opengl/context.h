#pragma once

#include <cheryl/export.h>

#include <core/rendering/presentation-surface.h>

namespace CE::Rendering::OpenGL {
    /** OpenGL context supplied by a compatible platform adapter, not by the renderer.
     * Context operations and resource uploads must run on the thread owning this context.
     */
    class CE_OPENGL_API Context : public ::CE::Rendering::PresentationSurface {
    public:
        using ProcAddress = void (*)(); ///< Opaque native procedure pointer; the renderer supplies typed loader casts.

        virtual void make_current() = 0;
        virtual void release_current() = 0;
        [[nodiscard]] virtual bool is_current() const = 0;
        [[nodiscard]] virtual ProcAddress proc_address(const char* name) const = 0;
    };
}

/**
 * @fn void CE::Rendering::OpenGL::Context::make_current()
 * @brief Make this borrowed presentation context current on its permitted owner thread.
 * Concrete platform adapters define errors; keep the window/context owner alive. Resource operations still validate their own domain/current-context requirement.
 */

/**
 * @fn void CE::Rendering::OpenGL::Context::release_current()
 * @brief Release this context from its permitted calling thread after native operations are finished. Does not destroy the display/window or retire resources.
 */

/**
 * @fn bool CE::Rendering::OpenGL::Context::is_current() const
 * @return Whether this adapter's native context is current on the calling thread; adapter thread restrictions still apply.
 */

/**
 * @fn CE::Rendering::OpenGL::Context::ProcAddress CE::Rendering::OpenGL::Context::proc_address(const char* name) const
 * @brief Resolve a native procedure for the graphics loader.
 * @param name NUL-terminated native entry-point name borrowed through the call.
 * @return Procedure pointer, or null when unavailable according to the platform adapter.
 * Use on the context owner during initialization; resolution does not initialize the renderer.
 */
