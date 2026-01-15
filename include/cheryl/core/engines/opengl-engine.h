#pragma once
#include <cgl.h>
#include <enums.h>
#include <core/display.h>
#include <templates/observed-variables.h>

#include <glm.hpp>

#include "abstract-engine.h"

/* Questions about the division between systems and their uses:
 *  * Camera
 *  * glEngine => glWrapper?
 *  * DisplaySystem
 *  * Renderer
 *  Camera will need information from DisplaySystem, which it can just cache from events
 *
 * glEngine definitely needs to become a proper Facade(pg 185)
 * likely to rename it and the base class (engine -> wrapper)
 *
 * ChatGPT recommended a Renderer interface, thereby meaning we'll implement an OpenGLRenderer to actualize it
 * and later a VulkanRenderer and DirectX renderer or whatever feels right and has time made for it
 *
 * Additionally, a Factory and enum should be implemented to instantiate the user's choice. Thus allowing them to
 * remain unaware of the actual class name if they choose.
 *
 * Along with these decisions, other things like shaders and textures may call for the creation of
 * abstractions/interfaces for these ideas, and implementations which call upon the correct backend.
 * i.e. a Shader abstraction/interface, GLSLProgram implementation integrated with OpenGL (opengl -> GLSL)s
 */

namespace CE::Engine {
    struct glEngine final : iEngine {
    private:
        // todo: move everything to a camera object
        ObservedVariable<Enum::gfx_mode> m_gMode;
        ObservedVariable<float> m_nearplane;
        ObservedVariable<float> m_farplane;
        ObservedVariable<glm::mat4> m_projectionMatrix;

    protected:
        void calculate_projection();

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
