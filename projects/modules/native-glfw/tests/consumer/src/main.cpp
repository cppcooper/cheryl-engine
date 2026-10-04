#include <cheryl/backends/native-glfw.h>
#include <cheryl/backends/native-glfw/diagnostics.h>

#include <type_traits>

static_assert(std::is_base_of_v<CE::iDisplaySystem, CE::DisplaySystem>);
static_assert(std::is_base_of_v<CE::iWindow, CE::Window>);
#if CHERYL_NATIVE_INPUT
static_assert(std::is_base_of_v<CE::Input::iInputSystem, CE::Input::InputSystem>);
#endif

int main() {
    CE::DisplayDetail::report_glfw_diagnostics("consumer");
    return 0;
}
