#include "developer-console.h"
#include "developer-console-model.h"
#include "developer-console-document.h"

namespace DemoConsole {
    View::View(CE::UI::RmlUi::Session& session, Model& model)
        : CE::Tools::Console::Presentation::View(session, model, {std::string{document}, {
            {"console-draft", "magic.edit \"spoken release via contact after 0 ms { bind hand = willing touched actor at release ; together ordered { on hand bridge Mend@1 { sela 5 hp tava-a } ; } }\""},
            {"console-quote", "magic.quote marin"},
            {"console-cast", "magic.draft.cast marin"}
        }}) {}
}
