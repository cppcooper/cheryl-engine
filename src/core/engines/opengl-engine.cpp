#include <core/engines/opengl-engine.h>

#include <core/controls/input-system.h>
#include <core/rendering/opengl-renderer.h>
#include <templates/singleton.h>

namespace CE::Engine {
    glEngine::glEngine() :
        RuntimeEngine(Singleton_CTS<RenderAPIs::OpenGLRenderer>::get(), []() -> Input::iInputSystem& {
            return Input::InputSystem::get();
        }) {}
}
