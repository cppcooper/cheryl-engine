#include <cheryl/text/font-service.h>
#include <cheryl/text/font-rasterizer.h>
#include <cheryl/text/layout.h>

bool exercise_fonts() {
    CE::Text::Font font;
    {
        CE::Text::FontService fonts;
        font = fonts.default_font();
    }
    const auto text = CE::Text::layout_text(font, "Consumer");
    const auto raster = CE::Text::FontRasterizer::load(font);
    return font.reference() == "cheryl:embedded" && !text.glyphs().empty() && raster.covers(U'A');
}
