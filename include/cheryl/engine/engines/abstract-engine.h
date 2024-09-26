#pragma once
#ifndef ABSTRACT_ENGINE_H
#define ABSTRACT_ENGINE_H
#include <enums.h>

namespace CE::Engine {
    struct iEngine {
        virtual ~iEngine() = default;
        virtual void init() = 0;
        virtual void deinit() = 0;
        virtual void pre_update() = 0;
        virtual void post_update() = 0;
        virtual void pre_draw() = 0;
        virtual void post_draw() = 0;

        virtual void set_mode(Enum::gfx_mode) = 0;
        virtual void set_mode(Enum::window_mode) = 0;
        virtual void set_clear_colour(float,float,float,float) = 0;
        virtual void hide_cursor(bool) = 0;
    };
}

#endif //ABSTRACT_ENGINE_H
