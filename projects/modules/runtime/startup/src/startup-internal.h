#pragma once

#include <application/startup.h>

namespace CE::Application::Detail {
    [[nodiscard]] Startup make_opengl_startup(CE::Input::System* input, std::string description, ApplicationOptions configuration);
}
