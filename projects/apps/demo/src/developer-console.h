#pragma once
#include <cheryl/tools/console/view.h>

namespace DemoConsole {
    class Model;
    class View final : public CE::Tools::Console::Presentation::View {
    public:
        View(CE::UI::RmlUi::Session& session, Model& model);
    };
}
