#include <gtest/gtest.h>

#include "../../../private/rasterizer-bridge.h"

#include <RmlUi/Core/Core.h>

#include <stdexcept>

TEST(ui_rmlui_font, callback_abi) {
    EXPECT_THROW(static_cast<void>(cheryl_rmlui_create_font_engine_v1(nullptr)), std::invalid_argument);
    CherylRmlUiPrivate::RasterizerCallbacks callbacks;
    callbacks.abi = 2;
    EXPECT_THROW(static_cast<void>(cheryl_rmlui_create_font_engine_v1(&callbacks)), std::invalid_argument);
    callbacks.abi = 1;
    callbacks.size = 0;
    EXPECT_THROW(static_cast<void>(cheryl_rmlui_create_font_engine_v1(&callbacks)), std::invalid_argument);
    callbacks.size = sizeof(callbacks);
    EXPECT_THROW(static_cast<void>(cheryl_rmlui_create_font_engine_v1(&callbacks)), std::invalid_argument);
    EXPECT_EQ(Rml::GetFontEngineInterface(), nullptr);
}
