#include <cheryl/backends/opengl.h>

#include <type_traits>

static_assert(std::is_base_of_v<CE::RenderAPIs::iRenderer, CE::RenderAPIs::OpenGLRenderer>);
static_assert(std::is_base_of_v<CE::Assets::ResourceProvider, CE::Assets::OpenGLResourceProvider>);

// Reference a real module symbol without opening a native display at execution.
using Factory = std::unique_ptr<CE::Engine::EngineContext> (*)(CE::Input::iInputSystem&, const CE::Engine::GlfwOpenGLConfig&);
Factory volatile factory = &CE::Engine::make_glfw_opengl_context;

int main() {
    return factory ? 0 : 1;
}
