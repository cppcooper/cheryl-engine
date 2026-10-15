#pragma once

#include <ui/rmlui/session.h>

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>

#include <stdexcept>

namespace RmlUiTests {
    inline Rml::ElementDocument& document(CE::UI::RmlUi::Session& session, const Rml::String& source_url = "[document from memory]") {
        if (!session.load_font(std::filesystem::path{CHERYL_RMLUI_TEST_FONT}, "proof"))
            throw std::runtime_error("Cannot load the real font fixture");
        session.set_view({320, 240}, {640, 480});
        auto* document = session.context().LoadDocumentFromMemory(R"(
            <rml><head><style>
                body { width: 100%; height: 100%; margin: 0; font-family: proof; font-size: 18px; }
                #label { position: absolute; left: 10px; top: 5px; color: #f0e0d0; }
                input { position: absolute; left: 10px; top: 35px; width: 180px; height: 30px;
                        background-color: #eeeeee; color: #111111; }
            </style></head><body><div id="label">Real document</div><input id="field" type="text" /></body></rml>
        )", source_url);
        if (!document)
            throw std::runtime_error("Cannot load the native document");
        document->Show();
        session.update_time(0);
        return *document;
    }

    inline Rml::ElementFormControlInput& field(Rml::ElementDocument& document) {
        auto* field = dynamic_cast<Rml::ElementFormControlInput*>(document.GetElementById("field"));
        if (!field)
            throw std::runtime_error("Missing native input element");
        return *field;
    }
}
