#include "font.h"

#include "../private/rasterizer-bridge.h"

namespace CE::UI::RmlUi::Detail {
    std::unique_ptr<Rml::FontEngineInterface> make_font_engine() {
        return std::unique_ptr<Rml::FontEngineInterface>{cheryl_rmlui_create_font_engine_v1(&rasterizer_callbacks())};
    }
}
