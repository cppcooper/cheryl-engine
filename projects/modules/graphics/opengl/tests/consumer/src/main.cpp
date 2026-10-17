#include <cheryl/backends/opengl.h>
#include <cheryl/core/logging/process-lifetime.h>

#include <type_traits>

static_assert(std::is_base_of_v<CE::Rendering::Renderer, CE::Rendering::OpenGL::Renderer>);
static_assert(std::is_base_of_v<CE::Rendering::Resources::Provider, CE::Rendering::OpenGL::ResourceProvider>);

// Reference a real module symbol without opening a native display at execution.
using Factory = std::unique_ptr<CE::Runtime::Context> (*)(CE::Input::System&, const CE::Application::GLFWOpenGL::Options&);
Factory volatile factory = &CE::Application::GLFWOpenGL::make_context;
using VectorSetter = void (CE::Rendering::OpenGL::Program::*)(const char*, const glm::vec3&);
VectorSetter volatile uniform_setter = &CE::Rendering::OpenGL::Program::set_uniform_vec;

int main() {
    CE::Logging::ProcessLifetime logging_lifetime;
    return factory && uniform_setter ? 0 : 1;
}
