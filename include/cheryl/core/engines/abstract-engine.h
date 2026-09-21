#pragma once
#include <enums.h>
#include <core/rendering/renderer.h>

namespace CE::Engine {
    struct iEngine {
        RenderAPIs::iRenderer* renderer = nullptr;
        virtual ~iEngine() = default;
        virtual void init() = 0;
        virtual void deinit() = 0;
        virtual void poll_input() = 0;
        virtual void pre_draw() = 0;
        virtual void post_draw() = 0;
        [[nodiscard]] virtual bool should_close() const = 0;

        virtual void set_mode(Enum::gfx_mode) = 0;
        virtual void set_mode(Enum::window_mode) = 0;
        virtual void set_clear_colour(float,float,float,float) = 0;
        virtual void hide_cursor(bool) = 0;
    };
}
