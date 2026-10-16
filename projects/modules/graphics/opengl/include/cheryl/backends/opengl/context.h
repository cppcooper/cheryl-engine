#pragma once

#include <core/rendering/presentation-surface.h>

namespace CE::Rendering::OpenGL {
    /** OpenGL context supplied by a compatible platform adapter, not by the renderer.
     * Context operations and resource uploads must run on the thread owning this context.
     */
    class Context : public ::CE::Rendering::PresentationSurface {
    public:
        using ProcAddress = void (*)();

        virtual void make_current() = 0;
        virtual void release_current() = 0;
        [[nodiscard]] virtual bool is_current() const = 0;
        [[nodiscard]] virtual ProcAddress proc_address(const char* name) const = 0;
    };
}
