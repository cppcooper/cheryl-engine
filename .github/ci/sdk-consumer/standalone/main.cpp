#include <cheryl/cheryl.h>
#include <cheryl/core/logging/logger.h>
#include <cheryl/core/logging/process-lifetime.h>

#if defined(SPDLOG_VER_MAJOR) || defined(GLFW_VERSION_MAJOR) || defined(GAINPUT_VERSION_MAJOR) || defined(GLAD_GL_H_) ||                   \
    defined(miniaudio_h)
#error A public SDK consumer must not inherit private native implementation headers.
#endif

#if CHERYL_HAS_UI_TGUI
#include <TGUI/Widgets/Button.hpp>
using ButtonFactory = tgui::Button::Ptr (*)(const tgui::String&);
ButtonFactory volatile widget_factory = &tgui::Button::create;
#endif
#if CHERYL_HAS_UI_RMLUI
#include <RmlUi/Core/Core.h>
#endif

namespace {
    constexpr char category[] = "standalone-sdk";
}

int main() {
    CE::Logging::ProcessLifetime logging;
    CE::Logging::Logger<category>::set_level_logger(CE::Logging::Level::off);
    if (CE::Logging::Logger<category>::should_log(CE::Logging::Level::warn))
        return 1;
    constexpr CE::Input::Control key{1, CE::Input::KeyboardKey::A};
    constexpr CE::Input::Control mouse{2, CE::Input::MouseButton::Left};
    constexpr CE::Input::Control pad{3, CE::Input::GamepadControl::A};
    if (key.control == CE::Input::invalid_control || mouse.control == CE::Input::invalid_control ||
        pad.control == CE::Input::invalid_control)
        return 2;
#if CHERYL_HAS_OPENGL
    using Setter = void (CE::Rendering::OpenGL::Program::*)(const char*, const glm::vec3&);
    Setter volatile setter = &CE::Rendering::OpenGL::Program::set_uniform_vec;
    if (!setter)
        return 3;
#endif
#if CHERYL_HAS_STARTUP
    using Factory = CE::Application::Startup (*)(std::string, CE::Application::ApplicationOptions);
    Factory volatile factory = &CE::Application::make_startup;
    if (!factory)
        return 4;
#endif
#if CHERYL_HAS_UI_TGUI
    if (!widget_factory)
        return 5;
#endif
#if CHERYL_HAS_UI_RMLUI
    if (Rml::GetVersion().empty())
        return 6;
#endif
#if CHERYL_HAS_AUDIO_MINIAUDIO
    auto audio = CE::Audio::Miniaudio::System::open_offline();
    audio->close();
    if (!audio->closed())
        return 7;
#endif
    return 0;
}
