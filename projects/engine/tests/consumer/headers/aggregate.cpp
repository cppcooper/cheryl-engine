#include <cheryl/cheryl.h>

#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

static_assert(sizeof(CE::Workers::Pool) > 0);
static_assert(sizeof(CE::Text::FontSelection) > 0);
static_assert(sizeof(CE::Text::Font) > 0);
static_assert(sizeof(CE::Text::FontService) > 0);
static_assert(sizeof(CE::Text::Rendering::Updater) > 0);
static_assert(sizeof(CE::Audio::Clip) > 0);
static_assert(sizeof(CE::Rendering::Frame) > 0);
static_assert(sizeof(CE::Input::Actions::Context) > 0);
static_assert(sizeof(CE::Input::Actions::ContextSubmission) > 0);

#if defined(CHERYL_CONSUMER_NATIVE_GLFW)
static_assert(sizeof(CE::Platform::GLFW::DisplaySystem) > 0);
static_assert(sizeof(CE::Platform::GLFW::Window) > 0);
#if CHERYL_NATIVE_INPUT
static_assert(sizeof(CE::Platform::GLFW::InputSystem) > 0);
#endif
#endif

#if defined(CHERYL_CONSUMER_DEBUG_TERMINAL_LINUX)
#if !defined(CHERYL_HAS_DEBUG_TERMINAL_LINUX) || !CHERYL_HAS_DEBUG_TERMINAL_LINUX
#error The aggregate target must expose the selected Linux debug terminal module.
#endif
#endif

#if defined(CHERYL_CONSUMER_OPENGL)
static_assert(sizeof(CE::Rendering::OpenGL::Renderer) > 0);
static_assert(sizeof(CE::Rendering::OpenGL::ResourceProvider) > 0);
#elif defined(GL_VERSION_3_3) || defined(GLFW_VERSION_MAJOR)
#error Consumers without the OpenGL module must not inherit graphics SDK headers.
#endif

#if defined(CHERYL_CONSUMER_UI_TGUI)
static_assert(sizeof(CE::UI::TGUI::Scene) > 0);
static_assert(sizeof(CE::UI::TGUI::Session) > 0);
#endif

#if defined(CHERYL_CONSUMER_UI_RMLUI)
static_assert(sizeof(CE::UI::RmlUi::Scene) > 0);
static_assert(sizeof(CE::UI::RmlUi::Session) > 0);
#endif

#if defined(CHERYL_CONSUMER_AUDIO_MINIAUDIO)
static_assert(sizeof(CE::Audio::Miniaudio::System) > 0);
#endif

#if defined(miniaudio_h)
#error Public audio headers must not include the native audio SDK.
#endif

#if defined(CHERYL_CONSUMER_SUPPORT_STARTUP)
static_assert(sizeof(CE::Application::Startup) > 0);
#endif

#if defined(CHERYL_CONSUMER_SUPPORT_DEBUG_TERMINAL)
static_assert(std::is_same_v<CE::Application::DebugTerminal::Main, int (*)(int, char**)>);
#endif

#if defined(CHERYL_CONSUMER_SUPPORT_WORKER_TOPOLOGY_HWLOC)
static_assert(std::is_same_v<decltype(CE::Workers::Hwloc::make_topology_provider()), std::shared_ptr<CE::Workers::TopologyProvider>>);
#endif

#if defined(CHERYL_CONSUMER_SUPPORT_WORKER_TOPOLOGY_JSON)
static_assert(std::is_same_v<decltype(CE::Workers::Json::parse_configuration(std::string_view{})), CE::Workers::Configuration>);
#endif

#if defined(CHERYL_CONSUMER_SUPPORT_OPENGL_STARTUP)
static_assert(
    std::is_same_v<decltype(CE::Application::GLFWOpenGL::make_startup(std::declval<CE::Input::System&>())), CE::Application::Startup>
);
#endif
