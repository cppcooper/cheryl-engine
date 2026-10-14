#include <cheryl/backends/opengl.h>

#include <type_traits>

static_assert(std::is_base_of_v<CE::Rendering::Renderer, CE::Rendering::OpenGL::Renderer>);
static_assert(std::is_base_of_v<CE::Rendering::Resources::Provider, CE::Rendering::OpenGL::ResourceProvider>);

// Reference a real module symbol without opening a native display at execution.
using Factory = std::unique_ptr<CE::Runtime::Context> (*)(CE::Input::System&, const CE::Application::GLFWOpenGL::Options&);
Factory volatile factory = &CE::Application::GLFWOpenGL::make_context;

int main() {
    return factory ? 0 : 1;
}
