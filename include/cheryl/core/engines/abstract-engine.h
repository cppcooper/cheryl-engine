#pragma once
#include <enums.h>
#include <core/rendering/renderer.h>

// template for storing/modifying a variable (T*, maybe T& actually), with a signal to notify of or listen for changes made through the template
// todo: this is done (i think): design transport for matrices (proj, view, model) to shaders

namespace CE::Engine {
    struct iEngine {
        RenderAPIs::iRenderer* renderer = nullptr;
        virtual ~iEngine() = default;
        virtual void init() = 0;
        virtual void deinit() = 0;
        virtual void pre_draw() = 0;
        virtual void post_draw() = 0;

        virtual void set_mode(Enum::gfx_mode) = 0;
        virtual void set_mode(Enum::window_mode) = 0;
        virtual void set_clear_colour(float,float,float,float) = 0;
        virtual void hide_cursor(bool) = 0;
    };
}
