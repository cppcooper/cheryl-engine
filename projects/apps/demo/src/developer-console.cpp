#include "developer-console.h"
#include "developer-console-model.h"

namespace DemoConsole {
    View::View(CE::UI::RmlUi::Session& session, Model& model)
        : CE::Tools::Console::Presentation::View(session, model) {}
}
