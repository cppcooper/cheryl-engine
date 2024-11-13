#pragma once
#ifndef GLENGINE_H
#define GLENGINE_H
#include <cgl.h>
#include <enums.h>
#include <engine/view/display-system.h>
#include <glm.hpp>
#include "abstract-engine.h"

namespace CE::Engine {
    struct glEngine final : iEngine {
    private:
        DisplaySystem display;
        Enum::gfx_mode m_gMode{};
        glm::mat4 m_viewMatrix{1.f};
        glm::mat4 m_projectionMatrix{};
        float m_nearplane{0.1f};
        float m_farplane{10000.f};

        union {
            std::array<float, 4> rgba{0.8f, 0.6f, 0.7f, 0.0f};
            struct {
                float r,g,b,a;
            } colour;
        }clear_colour{};
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
