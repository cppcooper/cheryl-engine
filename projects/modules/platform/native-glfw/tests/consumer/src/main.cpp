#include <cheryl/backends/native-glfw.h>
#include <cheryl/core/logging/process-lifetime.h>
#include <cheryl/backends/native-glfw/diagnostics.h>

#include <type_traits>

static_assert(std::is_base_of_v<CE::Platform::DisplaySystem, CE::Platform::GLFW::DisplaySystem>);
static_assert(std::is_base_of_v<CE::Platform::Window, CE::Platform::GLFW::Window>);
#if CHERYL_NATIVE_INPUT
static_assert(std::is_base_of_v<CE::Input::System, CE::Platform::GLFW::InputSystem>);
#endif

int main() {
    CE::Logging::ProcessLifetime logging_lifetime;
    CE::Platform::GLFW::Detail::report_glfw_diagnostics("consumer");
#if CHERYL_NATIVE_INPUT
    CE::Platform::GLFW::InputSystem input;
    if (input.device(input.keyboard_id()).known())
        return 1;
#endif
    return 0;
}
