#pragma once

#include <RmlUi/Core/FontEngineInterface.h>

#include <memory>

namespace CherylRmlUiPrivate {
    struct RasterizerCallbacks;
}

namespace CE::UI::RmlUi::Detail {
    [[nodiscard]] const CherylRmlUiPrivate::RasterizerCallbacks& rasterizer_callbacks();
    [[nodiscard]] std::unique_ptr<Rml::FontEngineInterface> make_font_engine();
}
