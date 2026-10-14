#include <cheryl/backends/native-glfw.h>
#include <cheryl/backends/native-glfw/diagnostics.h>

#include <type_traits>

static_assert(std::is_base_of_v<CE::Platform::DisplaySystem, CE::Platform::GLFW::DisplaySystem>);
static_assert(std::is_base_of_v<CE::Platform::Window, CE::Platform::GLFW::Window>);
#if CHERYL_NATIVE_INPUT
static_assert(std::is_base_of_v<CE::Input::System, CE::Platform::GLFW::InputSystem>);
#endif

int main() {
    CE::Platform::GLFW::Detail::report_glfw_diagnostics("consumer");
    return 0;
}
