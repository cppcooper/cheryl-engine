#pragma once
#ifndef GLENGINE_H
#define GLENGINE_H
#include <cgl.h>
#include <enums.h>
#include <core/display.h>
#include <templates/observed-variables.h>

#include <glm.hpp>

#include "abstract-engine.h"

namespace CE::Engine {
    struct glEngine final : iEngine {
    private:
        DisplaySystem display;
        ObservedVariable<Enum::gfx_mode,1> m_gMode;
        ObservedVariable<glm::mat4> m_viewMatrix;// viewMatrix represents the camera todo: move to camera controller
        ObservedVariable<glm::mat4> m_projectionMatrix;
        ObservedVariable<float> m_nearplane;
        ObservedVariable<float> m_farplane;

    protected:
        void update_matrices();

    public:
        glEngine();
        ~glEngine() override = default;
        void init() override;
        void deinit() override;
        void pre_draw() override;
        void post_draw() override;
        void set_mode(Enum::gfx_mode) override;
        void set_mode(Enum::window_mode) override;
        void set_clear_colour(float, float, float, float) override;
        void hide_cursor(bool) override;
    };
}
#endif //GLENGINE_H
